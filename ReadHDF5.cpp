/**
 * MOAB, a Mesh-Oriented datABase, is a software component for creating,
 * storing and accessing finite element mesh data.
 * 
 * Copyright 2004 Sandia Corporation.  Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Coroporation, the U.S. Government
 * retains certain rights in this software.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 */

//-------------------------------------------------------------------------
// Filename      : ReadHDF5.cpp
//
// Purpose       : TSTT HDF5 Writer 
//
// Special Notes : WriteSLAC used as template for this
//
// Creator       : Jason Kraftcheck
//
// Creation Date : 04/18/04
//-------------------------------------------------------------------------

#include <assert.h>
#include <H5Tpublic.h>
#include "MBInterface.hpp"
#include "MBInternals.hpp"
#include "ReadHDF5.hpp"
#include "WriteHDF5.hpp"

#undef DEBUG

#ifdef DEBUG
#  define DEBUGOUT(A) fputs( A, stderr )
#  include <stdio.h>
#else
#  define DEBUGOUT(A)
#endif

#define READ_HDF5_BUFFER_SIZE (40*1024*1024)

MBReaderIface* ReadHDF5::factory( MBInterface* iface )
  { return new ReadHDF5( iface ); }

ReadHDF5::ReadHDF5( MBInterface* iface )
  : bufferSize( READ_HDF5_BUFFER_SIZE ),
    dataBuffer( 0 ),
    iFace( iface ), 
    filePtr( 0 ), 
    readUtil( 0 ),
    handleType( 0 )
{
}

MBErrorCode ReadHDF5::init()
{
  MBErrorCode rval;

  if (readUtil) 
    return MB_SUCCESS;
  
  WriteHDF5::register_known_tag_types( iFace );
  
  handleType = H5Tcopy( H5T_NATIVE_ULONG );
  if (handleType < 0)
    return MB_FAILURE;
  
  if (H5Tset_size( handleType, sizeof(MBEntityHandle)) < 0)
  {
    H5Tclose( handleType );
    return MB_FAILURE;
  }
  
  rval = iFace->query_interface( "MBReadUtilIface", (void**)&readUtil );
  if (MB_SUCCESS != rval)
  {
    H5Tclose( handleType );
    return rval;
  }
  
  setSet.first_id = 0;
  setSet.type2 = mhdf_set_type_handle();
  setSet.type = MBENTITYSET;
  nodeSet.first_id = 0;
  nodeSet.type2 = mhdf_node_type_handle();
  nodeSet.type = MBVERTEX;
  
  return MB_SUCCESS;
}
  

ReadHDF5::~ReadHDF5()
{
  if (!readUtil) // init() failed.
    return;

  iFace->release_interface( "MBReadUtilIface", readUtil );
  H5Tclose( handleType );
}

MBErrorCode ReadHDF5::load_file( const char* filename, const int*, const int num_blocks )
{
  MBErrorCode rval;
  mhdf_Status status;
  std::string tagname;
  int num_tags = 0;
  char** tag_names = NULL;
  std::vector<mhdf_ElemHandle> groups;
  std::vector<mhdf_ElemHandle>::const_iterator g_itor;
  std::list<ElemSet>::iterator el_itor;
  int num_groups;
  MBEntityHandle all;  // meshset of everything in file.

  if (num_blocks)
    return MB_FAILURE;

  if (MB_SUCCESS != init())
    return MB_FAILURE;

DEBUGOUT( "Opening File\n" );  
  
    // Open the file
  filePtr = mhdf_openFile( filename, 0, NULL, &status );
  if (!filePtr)
  {
    readUtil->report_error( mhdf_message( &status ));
    return MB_FAILURE;
  }
  
  dataBuffer = (char*)malloc( bufferSize );
  if (!dataBuffer)
    goto read_fail;

DEBUGOUT("Reading Nodes.\n");
  
  if (read_nodes() != MB_SUCCESS)
    goto read_fail;

DEBUGOUT("Reading element connectivity.\n");

  num_groups = mhdf_numElemGroups( filePtr, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ));
    return MB_FAILURE;
  }

  if (num_groups)
  {
    groups.resize( num_groups );
    mhdf_getElemGroups( filePtr, &groups[0], &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ));
      return MB_FAILURE;
    }
  }
    
  for (g_itor = groups.begin(); g_itor != groups.end(); ++g_itor)
  {
    int poly = mhdf_isPolyElement( filePtr, *g_itor, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ));
      return MB_FAILURE;
    }

    if (poly)
      rval = read_poly( *g_itor );
    else
      rval = read_elems( *g_itor );
      
    if (MB_SUCCESS != rval)
      goto read_fail;
  }    
  
DEBUGOUT("Reading sets.\n");
  
  if (read_sets() != MB_SUCCESS)
    goto read_fail;

DEBUGOUT("Reading adjacencies.\n");
  
  if (read_adjacencies( nodeSet ) != MB_SUCCESS)
    goto read_fail;
  for (el_itor = elemList.begin(); el_itor != elemList.end(); ++el_itor)
    if (read_adjacencies( *el_itor ) != MB_SUCCESS)
      goto read_fail;

DEBUGOUT("Reading tags.\n");
  
  tag_names = mhdf_getTagNames( filePtr, &num_tags, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ));
    return MB_FAILURE;
  }
  
  for (int t = 0; t < num_tags; ++t)
  {
    rval = read_tag( tag_names[t] );
    free( tag_names[t] );
    tag_names[t] = NULL;
    if (MB_SUCCESS != rval)
      goto read_fail;
  }
  free( tag_names );
  tag_names = 0;
  
DEBUGOUT("Finishing read.\n");
  if (MB_SUCCESS != read_qa( all ))
    goto read_fail;

    // Clean up and exit.
  free( dataBuffer );
  dataBuffer = 0;
  mhdf_closeFile( filePtr, &status );
  filePtr = 0;
  return MB_SUCCESS;
  
read_fail:
  
  if (dataBuffer)
  {
    free( dataBuffer );
    dataBuffer = 0;
  }
  
  if (tag_names)
  {
    for (int tt = 0; tt < num_tags; ++tt)
      if (NULL != tag_names[tt])
        free( tag_names[tt] );
    free( tag_names );
  }
  
  mhdf_closeFile( filePtr, &status );
  filePtr = 0;
  
    /* Destroy any mesh that we've read in */
  if (!setSet.range.empty())
  {
    iFace->clear_meshset( setSet.range );
    iFace->delete_entities( setSet.range );
    setSet.range.clear();
  }
  for (el_itor = elemList.begin(); el_itor != elemList.end(); ++el_itor)
  {
    if (!el_itor->range.empty())
      iFace->delete_entities( el_itor->range );
  }
  elemList.clear();
  if (!nodeSet.range.empty())
  {
    iFace->delete_entities( nodeSet.range );
    nodeSet.range.clear();
  }
  
  return MB_FAILURE;
}

MBErrorCode ReadHDF5::read_nodes()
{
  MBErrorCode rval;
  mhdf_Status status;
  long count, first_id;
  int dim;
  MBRange range;
  
  int cdim;
  rval = iFace->get_dimension( cdim );
  if (MB_SUCCESS != rval)
    return rval;
  
  hid_t data_id = mhdf_openNodeCoords( filePtr, &count, &dim, &first_id, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ));
    return MB_FAILURE;
  }
  
  if (cdim < dim)
  {
    rval = iFace->set_dimension( dim );
    if (MB_SUCCESS != rval)
      return rval;
  }
  
  MBEntityHandle handle;
  std::vector<double*> arrays(dim);
  rval = readUtil->get_node_arrays( dim, (int)count, (int)first_id, handle, arrays );
  if (MB_SUCCESS != rval)
  {
    mhdf_closeData( filePtr, data_id, &status );
    return rval;
  }
  
  nodeSet.range.clear();
  nodeSet.range.insert( handle, handle + count - 1 );
  nodeSet.first_id = first_id;
  nodeSet.type = MBVERTEX;
  nodeSet.type2 = mhdf_node_type_handle();
  for (int i = 0; i < dim; i++)
  {
    mhdf_readNodeCoord( data_id, 0, count, i, arrays[i], &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message(&status) );
      mhdf_closeData( filePtr, data_id, &status );
      return MB_FAILURE;
    }
  }
  for (int j = dim; j < cdim; j++)
    bzero( arrays[j], count * sizeof(double) );
  
  mhdf_closeData( filePtr, data_id, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message(&status) );
    return MB_FAILURE;
  }
  
  return MB_SUCCESS;
}

MBErrorCode ReadHDF5::read_elems( mhdf_ElemHandle elem_group )
{
  MBErrorCode rval;
  mhdf_Status status;
  char name[64];
  
    // Put elem set in list early so clean up code can 
    // get rid of them if we fail.
  ElemSet empty_set;
  empty_set.type2 = elem_group;
  elemList.push_back( empty_set );
  std::list<ElemSet>::iterator it = elemList.end();
  --it;
  ElemSet& elems = *it;
  
  mhdf_getElemTypeName( filePtr, elem_group, name, sizeof(name), &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  elems.type = MBCN::EntityTypeFromName( name );
  if (elems.type == MBMAXTYPE)
  {
    readUtil->report_error( "Unknown element type: \"%s\".\n", name );
    return MB_FAILURE;
  }
  
  int nodes_per_elem;
  long count, first_id;
  hid_t data_id = mhdf_openConnectivity( filePtr, 
                                         elem_group,
                                         &nodes_per_elem,
                                         &count,
                                         &first_id,
                                         &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  elems.first_id = first_id;
  
  MBEntityHandle handle;
  MBEntityHandle* array;
  rval = readUtil->get_element_array( (int)count,
                                       nodes_per_elem,
                                       elems.type,
                                       (int)first_id,
                                       handle, 
                                       array );
  if (MB_SUCCESS != rval)
  {
    mhdf_closeData( filePtr, data_id, &status );
    return rval;
  }
  
  elems.range.insert( handle, handle + count - 1 );
  mhdf_readConnectivity( data_id, 0, count, handleType, array, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    mhdf_closeData( filePtr, data_id, &status );
    return MB_FAILURE;
  }
  
  mhdf_closeData( filePtr, data_id, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  rval = convert_id_to_handle( nodeSet, array, (size_t)(nodes_per_elem*count) );
  return rval;
}

MBErrorCode ReadHDF5::read_poly( mhdf_ElemHandle elem_group )
{
  MBErrorCode rval;
  mhdf_Status status;
  char name[64];
  
    // Put elem set in list early so clean up code can 
    // get rid of them if we fail.
  ElemSet empty_set;
  empty_set.type2 = elem_group;
  elemList.push_back( empty_set );
  std::list<ElemSet>::iterator it = elemList.end();
  --it;
  ElemSet& elems = *it;
  
  mhdf_getElemTypeName( filePtr, elem_group, name, sizeof(name), &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }

  elems.type = MBCN::EntityTypeFromName( name );
  if (elems.type == MBMAXTYPE)
  {
    readUtil->report_error( "Unknown element type: \"%s\".\n", name );
    return MB_FAILURE;
  }

  long count, first_id, data_len;
  hid_t handles[2];
  mhdf_openPolyConnectivity( filePtr, elem_group, &count, &data_len,
                             &first_id, handles, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  elems.first_id = first_id;
  
  MBEntityHandle handle;
  MBEntityHandle* conn_array;
  int* index_array;
  rval = readUtil->get_poly_element_array( count, data_len, elems.type, 
                             first_id, handle, index_array, conn_array );
  if (MB_SUCCESS != rval)
  {
    mhdf_closeData( filePtr, handles[0], &status );
    mhdf_closeData( filePtr, handles[1], &status );
    return rval;
  }
  elems.range.insert( handle, handle + count - 1 );
  
  mhdf_readPolyConnIndices( handles[0], 0, count, H5T_NATIVE_INT,  
                            index_array, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    mhdf_closeData( filePtr, handles[0], &status );
    mhdf_closeData( filePtr, handles[1], &status );
    return MB_FAILURE;
  }
  
  mhdf_readPolyConnIDs( handles[1], 0, data_len, handleType,
                        conn_array, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    mhdf_closeData( filePtr, handles[0], &status );
    mhdf_closeData( filePtr, handles[1], &status );
    return MB_FAILURE;
  }
  
  mhdf_closeData( filePtr, handles[0], &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    mhdf_closeData( filePtr, handles[0], &status );
    return MB_FAILURE;
  }
  mhdf_closeData( filePtr, handles[1], &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  return convert_id_to_handle( conn_array, (size_t)data_len );
}


MBErrorCode ReadHDF5::read_sets()
{
  MBErrorCode rval;
  mhdf_Status status;
  hid_t meta_id, data_id, child_id;
  MBEntityHandle prev_handle = 0;
  
    // Check what data is in the file for sets
  int have_sets, have_data, have_children;
  have_sets = mhdf_haveSets( filePtr, &have_data, &have_children, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }

  if (!have_sets)
    return MB_SUCCESS;
  
    // Open the list of sets
  long num_sets, first_id;
  meta_id = mhdf_openSetMeta( filePtr, &num_sets, &first_id, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  setSet.first_id = first_id;
  setSet.type = MBENTITYSET;
  setSet.type2 = mhdf_set_type_handle();
  
    // Create all the sets (empty)
    // Must do this before any set children/contents are read
    // to ensure that any sets referred to in the contents or
    // child list exist.
  
    // Iterate over sets one at a time
  long i, set_data[3];
  for (i = 0; i < num_sets; i++)  // for each set
  {
      // Get set description
    mhdf_readSetMeta( meta_id, i, 1, H5T_NATIVE_LONG, set_data, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, meta_id, &status );
      return MB_FAILURE;
    }
     
      // Clear ranged-storage bit.  It is internal data, not one
      // of MOABs set flags.
    set_data[2] &= ~(long)mhdf_SET_RANGE_BIT;
    
      // Create the set
    MBEntityHandle handle;
    rval = iFace->create_meshset( set_data[2], handle );
    if (MB_SUCCESS != rval)
    {
      mhdf_closeData( filePtr, meta_id, &status );
      return rval;
    }
    
    assert( handle > prev_handle && (prev_handle = handle));
    setSet.range.insert( handle );
  }

  
    // Open the list of set contents
  long data_len = 0;
  data_id = -1;
  if (have_data)
  {
    data_id = mhdf_openSetData( filePtr, &data_len, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, meta_id, &status );
      return MB_FAILURE;
    }
  }
  
    // Open the list of set children
  long child_len = 0;
  child_id = -1;
  if (have_children)
  {
    child_id = mhdf_openSetChildren( filePtr, &child_len, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, meta_id, &status );
      mhdf_closeData( filePtr, data_id, &status );
      return MB_FAILURE;
    }
  }
  
    // Set up buffer for set contents
  size_t chunk_size = bufferSize / sizeof(MBEntityHandle);
  if (chunk_size % 2)
    --chunk_size; // makes reading range data easier.
  MBEntityHandle* buffer = (MBEntityHandle*)dataBuffer;
  
    // Iterate over sets one at a time
  long data_offset = 0, child_offset = 0;
  MBRange range;
  MBRange::const_iterator set_iter = setSet.range.begin();
  for (i = 0; i < num_sets; i++)  // for each set
  {
      // Get set description
    mhdf_readSetMeta( meta_id, i, 1, H5T_NATIVE_LONG, set_data, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, meta_id, &status );
      mhdf_closeData( filePtr, data_id, &status );
      mhdf_closeData( filePtr, child_id, &status );
      return MB_FAILURE;
    }
    
      // Check if set contents are stored as ranges or a simple list
    bool ranged = (0 != (set_data[2] & (long)mhdf_SET_RANGE_BIT));
   
      // Read set contents
      
      // Check if we are reading past the end of the data
      // (shouldn't happen if file is vaild.)
      // Note: this will also catch the case where the set
      // contents list didn't exist, as data_len will be zero.
    if (set_data[0] >= data_len)
      { assert(0); return MB_FAILURE; }

      // Loop until all the entities in the set are read.
      // The buffer is rather large, so it is unlikely that
      // we'll loop more than once.
    if (data_offset > (set_data[0] + 1))
      { assert(0); return MB_FAILURE; }
    size_t remaining = set_data[0] + 1 - data_offset;
    while (remaining)
    {
      size_t count = remaining > chunk_size ? chunk_size : remaining;
      remaining -= count;
      mhdf_readSetData( data_id, data_offset, count, handleType, buffer, &status );
      if (mhdf_isError( &status ))
      {
        readUtil->report_error( mhdf_message( &status ) );
        mhdf_closeData( filePtr, meta_id, &status );
        mhdf_closeData( filePtr, data_id, &status );
        mhdf_closeData( filePtr, child_id, &status );
        return MB_FAILURE;
      }
      data_offset += count;

      if (ranged)
      {
        assert(count % 2 == 0);
        range.clear();
        rval = convert_range_to_handle( buffer, count / 2, range );
        if (MB_SUCCESS == rval)
          rval = iFace->add_entities( *set_iter, range );
      }
      else
      {
        rval = convert_id_to_handle( buffer, count );
        if (MB_SUCCESS == rval)
          rval = iFace->add_entities( *set_iter, buffer, count );
      }

      if (MB_SUCCESS != rval)
      {
        mhdf_closeData( filePtr, meta_id, &status );
        mhdf_closeData( filePtr, data_id, &status );
        mhdf_closeData( filePtr, child_id, &status );
        return rval;
      }
    } // while(remaining)
        
    
      // Read set children
    
      // Check if we are reading past the end of the data
      // (shouldn't happen if file is vaild.)
      // Note: this will also catch the case where the set
      // contents list didn't exist, as data_len will be zero.
    if (set_data[1] >= child_len)
      { assert(0); return MB_FAILURE; }

      // Loop until all the children are read.
      // The buffer is rather large, so it is unlikely that
      // we'll loop more than once.
    if (child_offset > (set_data[1] + 1))
      { assert(0); return MB_FAILURE; }
    remaining = set_data[1] + 1 - child_offset;
    while (remaining)
    {
      size_t count = remaining > chunk_size ? chunk_size : remaining;
      remaining -= count;
      mhdf_readSetChildren( child_id, child_offset, count, handleType, buffer, &status );
      if (mhdf_isError( &status ))
      {
        readUtil->report_error( mhdf_message( &status ) );
        mhdf_closeData( filePtr, meta_id, &status );
        mhdf_closeData( filePtr, data_id, &status );
        mhdf_closeData( filePtr, child_id, &status );
        return MB_FAILURE;
      }
      child_offset += count;

      rval = convert_id_to_handle( setSet, buffer, count );
      if (MB_SUCCESS != rval)
      {
        mhdf_closeData( filePtr, meta_id, &status );
        mhdf_closeData( filePtr, data_id, &status );
        mhdf_closeData( filePtr, child_id, &status );
        return rval;
      }

      rval = MB_SUCCESS;
      for (size_t j = 0; MB_SUCCESS == rval && j < count; j++)
        rval = iFace->add_child_meshset( *set_iter, buffer[j] );
      if (MB_SUCCESS != rval)
      {
        mhdf_closeData( filePtr, meta_id, &status );
        mhdf_closeData( filePtr, data_id, &status );
        mhdf_closeData( filePtr, child_id, &status );
        return rval;
      }
    } // while(remaining)
    
    ++set_iter;
  } // for (meshsets)
  
  
    // Close open data tables and return
  
  int error = 0;
  mhdf_closeData( filePtr, meta_id, &status );
  if (mhdf_isError( &status ))
    { readUtil->report_error( mhdf_message( &status ) ); error = 1; }
  if (have_data)
    mhdf_closeData( filePtr, data_id, &status );
  if (mhdf_isError( &status ))
    { readUtil->report_error( mhdf_message( &status ) ); error = 1; }
  if (have_children)
    mhdf_closeData( filePtr, child_id, &status );
  if (mhdf_isError( &status ))
    { readUtil->report_error( mhdf_message( &status ) ); error = 1; }
  
  return error ? MB_FAILURE : MB_SUCCESS;
}


MBErrorCode ReadHDF5::read_adjacencies( ElemSet& elems )
{
  MBErrorCode rval;
  mhdf_Status status;
  hid_t table;
  long data_len;
  
  int adj = mhdf_haveAdjacency( filePtr, elems.type2, &status );
  if (mhdf_isError(&status))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  if (!adj)
    return MB_SUCCESS;
  
  table = mhdf_openAdjacency( filePtr, elems.type2, &data_len, &status );
  if (mhdf_isError(&status))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  MBEntityHandle* buffer = (MBEntityHandle*)dataBuffer;
  size_t chunk_size = bufferSize / sizeof(MBEntityHandle);
  size_t remaining = data_len;
  size_t leading = 0;
  size_t offset = 0;
  while (remaining)
  {
    size_t count = remaining > chunk_size ? chunk_size : remaining;
    count -= leading;
    remaining -= count;
    
    mhdf_readAdjacency( table, offset, count, handleType, buffer + leading, &status );
    if (mhdf_isError(&status))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, table, &status );
      return MB_FAILURE;
    }
    
    MBEntityHandle* iter = buffer;
    MBEntityHandle* end = buffer + count + leading;
    while (end - iter >= 3)
    {
      rval = convert_id_to_handle( elems, iter, 1 );
      MBEntityHandle entity = *iter;
      MBEntityHandle count = *++iter;
      if (MB_SUCCESS != rval || count < 1)
      {
        assert(0);
        mhdf_closeData( filePtr, table, &status );
        return rval == MB_SUCCESS ? MB_FAILURE : rval;
      }
      ++iter;
      
      if (end < count + iter)
      {
        iter -= 2;
        break;
      }
      
      rval = convert_id_to_handle( iter, count );
      if (MB_SUCCESS != rval)
      {
        assert(0);
        mhdf_closeData( filePtr, table, &status );
        return rval;
      }
      
      rval = iFace->add_adjacencies( entity, iter, count, false );
      if (MB_SUCCESS != rval)
      {
        assert(0);
        mhdf_closeData( filePtr, table, &status );
        return rval;
      }
      
      iter += count;
    }
    
    leading = end - iter;
    memmove( buffer, iter, leading );
  }
  
  assert(!leading);  // unexpected truncation of data
  
  mhdf_closeData( filePtr, table, &status );
  if (mhdf_isError(&status))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  return MB_SUCCESS;  
}


MBErrorCode ReadHDF5::read_tag( const char* name )
{
  MBErrorCode rval;
  mhdf_Status status;
  MBTag type_handle;
  hid_t hdf_tag_type;
  MBTag handle;
 
  bool have_type = false;
  std::string tag_type_name = "__hdf5_tag_type_";
  tag_type_name += name;
  rval = iFace->tag_get_handle( tag_type_name.c_str(), type_handle );
  if (MB_SUCCESS == rval)
  {
    rval = iFace->tag_get_data( type_handle, 0, 0, &hdf_tag_type );
    if (MB_SUCCESS == rval)
      have_type = true;
    else if (MB_TAG_NOT_FOUND != rval)
      return rval;
  }
  else if (MB_TAG_NOT_FOUND != rval)
    return rval;
 
  int storage_size, have_global, have_default, is_opaque, have_sparse, tstt_class, num_bits;
  hid_t storage_type;
  mhdf_getTagInfo( filePtr, name, &storage_size, 
                   &have_default, &have_global, 
                   &is_opaque, &have_sparse, &tstt_class, 
                   &num_bits, &storage_type, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  int create_size = storage_size;
  int read_size = storage_size;
  hid_t read_type;
  if (have_type)
  {
    create_size = read_size = H5Tget_size(hdf_tag_type);
    if (!create_size) return MB_FAILURE;
    read_type = hdf_tag_type;
  }
  else if (tstt_class == MB_TAG_BIT)
  {
    if (!num_bits || num_bits > 8)
    {
      readUtil->report_error( "Invalid bit tag:  class is MB_TAG_BIT, num bits = %d\n", num_bits );
      return MB_FAILURE;
    }
    
    create_size = num_bits;
    read_size = 1;
    read_type = H5T_NATIVE_B8;
  }
  else if (is_opaque)
  {
    create_size = read_size = storage_size;
    read_type = 0;
  }
  else
  {
    rval = iFace->tag_get_handle( name, handle );
    if (rval == MB_TAG_NOT_FOUND)
      create_size = storage_size;
    else if (rval != MB_SUCCESS)
      return rval;
    else if (iFace->tag_get_size( handle, create_size ) != MB_SUCCESS)
      return MB_FAILURE;
    read_size = create_size;
    read_type = mhdf_getNativeType( storage_type, read_size, &status );
    if (mhdf_isError(&status))
    {
      readUtil->report_error( mhdf_message( &status ) );
      return MB_FAILURE;
    }
  }      
  
  if (have_default || have_global)
  {
    assert( 3*read_size < bufferSize );
    mhdf_getTagValues( filePtr, name, read_type, dataBuffer, dataBuffer + read_size, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      return MB_FAILURE;
    }
  }

  rval = iFace->tag_get_handle( name, handle );
  if (MB_TAG_NOT_FOUND == rval)
    rval = iFace->tag_create( name, create_size, (MBTagType)tstt_class, handle,
                              have_default ? dataBuffer : 0 );
  if (MB_SUCCESS != rval)
    return rval;
  
  int esize;
  rval = iFace->tag_get_size( handle, esize );
  if (MB_SUCCESS != rval)
    return rval;
  if (tstt_class != MB_TAG_BIT && esize != read_size)
    return MB_FAILURE;
  MBTagType etype;
  rval = iFace->tag_get_type( handle, etype );
  if (MB_SUCCESS != rval)
    return rval;
  if ((tstt_class ==  MB_TAG_BIT || etype == MB_TAG_BIT) && (tstt_class != etype))
    return MB_FAILURE;
  
  if (have_global)
  {
    rval = iFace->tag_set_data( handle, 0, 0, dataBuffer + read_size );
    if (MB_SUCCESS != rval)
      return rval;
  }
  
  MBErrorCode tmp = MB_SUCCESS;
  if (have_sparse)
    tmp = read_sparse_tag( handle, read_type, read_size );
  rval = read_dense_tag( handle, read_type, read_size );
  
  return MB_SUCCESS == tmp ? rval : tmp;
}

MBErrorCode ReadHDF5::read_dense_tag( MBTag tag_handle,
                                      hid_t hdf_read_type,
                                      size_t read_size )
{
  std::list<ElemSet>::iterator iter;
  const std::list<ElemSet>::iterator end = elemList.end();
  mhdf_Status status;
  std::string name;
  MBErrorCode rval;
  int have;
  
  rval = iFace->tag_get_name( tag_handle, name );
  if (MB_SUCCESS != rval)
    return rval;
  
  have = mhdf_haveDenseTag( filePtr, name.c_str(), nodeSet.type2, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  if (have)
  {
    rval = read_dense_tag( nodeSet, tag_handle, hdf_read_type, read_size );
    if (!rval)
      return rval;
  }
  
   
  have = mhdf_haveDenseTag( filePtr, name.c_str(), setSet.type2, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  if (have)
  {
    rval = read_dense_tag( setSet, tag_handle, hdf_read_type, read_size );
    if (!rval)
      return rval;
  }
  
  
  for (iter = elemList.begin(); iter != end; ++iter)
  {
    have = mhdf_haveDenseTag( filePtr, name.c_str(), iter->type2, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      return MB_FAILURE;
    }
    
    if (have)
    {
      rval = read_dense_tag( *iter, tag_handle, hdf_read_type, read_size );
      if (!rval)
        return rval;
    }
  }

  return MB_SUCCESS;
}
  
  
MBErrorCode ReadHDF5::read_dense_tag( ElemSet& set,
                                      MBTag tag_handle,
                                      hid_t hdf_read_type,
                                      size_t read_size )
{
  mhdf_Status status;
  std::string name;
  MBErrorCode rval;
  long num_values;
  
  rval = iFace->tag_get_name( tag_handle, name );
  if (MB_SUCCESS != rval)
    return rval;
  
  hid_t data = mhdf_openDenseTagData( filePtr, name.c_str(), 
                                      set.type2, &num_values, &status );
  if (mhdf_isError( &status ) )
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  if ((unsigned long)num_values != set.range.size())
  {
    assert( 0 );
    return MB_FAILURE;
  }
  
  MBRange::const_iterator iter = set.range.begin();
  const MBRange::const_iterator end = set.range.end();
  MBRange subrange;
  
  assert ((hdf_read_type == 0) || (H5Tget_size(hdf_read_type) == read_size));
  size_t chunk_size = bufferSize / read_size;
  size_t remaining = set.range.size();
  size_t offset = 0;
  while (remaining)
  {
    size_t count = remaining > chunk_size ? chunk_size : remaining;
    remaining -= count;
    
    MBRange::const_iterator stop = iter;
    stop += count;
    subrange.clear();
    subrange.merge( iter, stop );
    iter = stop;
    
    mhdf_readDenseTag( data, offset, count, hdf_read_type, dataBuffer, &status );
    offset += count;
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, data, &status );
      return MB_FAILURE;
    }
    
    rval = iFace->tag_set_data( tag_handle, subrange, dataBuffer );
    if (MB_SUCCESS != rval)
    {
      mhdf_closeData( filePtr, data, &status );
      return MB_FAILURE;
    }
  }
  
  mhdf_closeData( filePtr, data, &status );
  if (mhdf_isError( &status ) )
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }

  return MB_SUCCESS;
}


MBErrorCode ReadHDF5::read_sparse_tag( MBTag tag_handle,
                                       hid_t hdf_read_type,
                                       size_t read_size )
{
  mhdf_Status status;
  std::string name;
  MBErrorCode rval;
  long num_values;
  hid_t data[2];
  MBTagType mbtype;
  assert ((hdf_read_type == 0) || (H5Tget_size(hdf_read_type) == read_size));
  
  rval = iFace->tag_get_name( tag_handle, name );
  if (MB_SUCCESS != rval)
    return rval;
  
  rval = iFace->tag_get_type( tag_handle, mbtype );
  if (MB_SUCCESS != rval)
    return rval;
  
  mhdf_openSparseTagData( filePtr, name.c_str(), &num_values, data, &status );
  if (mhdf_isError( &status ) )
  {
    readUtil->report_error( mhdf_message( &status ) );
    return MB_FAILURE;
  }
  
  size_t chunk_size = (bufferSize - read_size) / (sizeof(MBEntityHandle) + read_size);
  MBEntityHandle* idbuffer = (MBEntityHandle*)dataBuffer;
  char* databuffer = dataBuffer + (chunk_size * read_size);
    // be careful about alignment
  if ((size_t)databuffer % read_size)
    databuffer += read_size - ((size_t)databuffer % read_size);
  
  size_t remaining = (size_t)num_values;
  size_t offset = 0;
  while (remaining)
  {
    size_t count = remaining > chunk_size ? chunk_size : remaining;
    remaining -= count;
    
    mhdf_readSparseTagEntities( data[0], offset, count, handleType, idbuffer, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, data[0], &status );
      mhdf_closeData( filePtr, data[1], &status );
      return MB_FAILURE;
    }
    
    mhdf_readSparseTagValues( data[1], offset, count, hdf_read_type, databuffer, &status );
    if (mhdf_isError( &status ))
    {
      readUtil->report_error( mhdf_message( &status ) );
      mhdf_closeData( filePtr, data[0], &status );
      mhdf_closeData( filePtr, data[1], &status );
      return MB_FAILURE;
    }
    
    offset += count;
    
    rval = convert_id_to_handle( idbuffer, count );
    if (MB_SUCCESS != rval)
    {
      mhdf_closeData( filePtr, data[0], &status );
      mhdf_closeData( filePtr, data[1], &status );
      return rval;
    }

/*** FIX ME - need to do one at a time for BIT tags!  This is stupid. ***/
    if (mbtype == MB_TAG_BIT)
    {
      rval = MB_SUCCESS;
      for (size_t i = 0; MB_SUCCESS == rval && i < count; ++i)
        rval = iFace->tag_set_data( tag_handle, idbuffer + i, 1, databuffer + i );
    }
    else
    {
      rval = iFace->tag_set_data( tag_handle, idbuffer, count, databuffer );
    }
    if (MB_SUCCESS != rval)
    {
      mhdf_closeData( filePtr, data[0], &status );
      mhdf_closeData( filePtr, data[1], &status );
      return rval;
    }
  }
  
  mhdf_closeData( filePtr, data[0], &status );
  if (mhdf_isError( &status ) )
    readUtil->report_error( mhdf_message( &status ) );
  mhdf_closeData( filePtr, data[1], &status );
  if (mhdf_isError( &status ) )
    readUtil->report_error( mhdf_message( &status ) );

  return MB_SUCCESS;
}

MBErrorCode ReadHDF5::convert_id_to_handle( const ElemSet& elems,
                                            MBEntityHandle* array,
                                            size_t size )
{
  MBEntityHandle offset = elems.first_id;
  MBEntityHandle last = offset + elems.range.size();
  MBEntityHandle *const end = array + size;
  while (array != end)
  {
    if (*array >= last || *array < (MBEntityHandle)offset)
      return MB_FAILURE;
    MBRange:: const_iterator iter = elems.range.begin();
    iter += *array - offset;
    *array = *iter;
    ++array;
  }
  
  return MB_SUCCESS;
}

MBErrorCode ReadHDF5::convert_id_to_handle( MBEntityHandle* array, 
                                            size_t size )
{
  MBEntityHandle *const end = array + size;
  MBEntityHandle offset = 1;
  MBEntityHandle last = 0;
  ElemSet* set = 0;
  std::list<ElemSet>::iterator iter;
  const std::list<ElemSet>::iterator i_end = elemList.end();

  while (array != end)
  {
    if (nodeSet.first_id && (*array < offset || *array >= last))
    {
      offset = nodeSet.first_id;
      last = offset + nodeSet.range.size();
      set = &nodeSet;
    }
    if (setSet.first_id && (*array < offset || *array >= last))
    {
      offset = setSet.first_id;
      last = offset + setSet.range.size();
      set = &setSet;
    }
    iter = elemList.begin();
    while (*array < offset || *array >= last)
    {
      if (iter == i_end)
      {
        return MB_FAILURE;
      }
      
      set = &*iter;
      offset = set->first_id;
      last = offset + set->range.size();
      ++iter;
    }
  
    MBRange:: const_iterator riter = set->range.begin();
    riter += *array - offset;
    *array = *riter;
    ++array;
  }
  
  return MB_SUCCESS;
}

MBErrorCode ReadHDF5::convert_range_to_handle( const MBEntityHandle* array,
                                               size_t num_ranges,
                                               MBRange& range )
{
  const MBEntityHandle *const end = array + 2*num_ranges;
  MBEntityHandle offset = 1;
  MBEntityHandle last = 0;
  ElemSet* set = 0;
  std::list<ElemSet>::iterator iter;
  const std::list<ElemSet>::iterator i_end = elemList.end();
  MBEntityHandle start = *(array++);
  MBEntityHandle count = *(array++);
  
  while (true)
  {
    if (nodeSet.first_id && (start < offset || start >= last))
    {
      offset = nodeSet.first_id;
      last = offset + nodeSet.range.size();
      set = &nodeSet;
    }
    if (setSet.first_id && (start < offset || start >= last))
    {
      offset = setSet.first_id;
      last = offset + setSet.range.size();
      set = &setSet;
    }
    iter = elemList.begin();
    while (start < offset || start >= last)
    {
      if (iter == i_end)
      {
        return MB_FAILURE;
      }
      
      set = &*iter;
      offset = set->first_id;
      last = offset + set->range.size();
      ++iter;
    }
  
    MBEntityHandle s_rem = set->range.size() - (start - offset);
    MBEntityHandle num = count > s_rem ? s_rem : count;
    MBRange::const_iterator riter = set->range.begin();
    riter += (start - offset);
    MBRange::const_iterator rend = riter;
    rend += num;
    assert( riter != rend );
    MBEntityHandle h_start = *riter++;
    MBEntityHandle h_prev = h_start;
    
    while (riter != rend)
    {
      if (h_prev + 1 != *riter)
      {
        range.insert( h_start, h_prev );
        h_start = *riter;
      }
      h_prev = *riter;
      ++riter;
    }
    range.insert( h_start, h_prev );
    
    count -= num;
    start += num;
    if (count == 0)
    {
      if (array == end)
        break;
      
      start = *(array++);
      count = *(array++);
    }
  }
  
  return MB_SUCCESS;
}
  

MBErrorCode ReadHDF5::read_qa( MBEntityHandle& import_set )
{
  MBErrorCode rval;
  mhdf_Status status;
  std::vector<std::string> qa_list;
  
  int qa_len;
  char** qa = mhdf_readHistory( filePtr, &qa_len, &status );
  if (mhdf_isError( &status ))
  {
    readUtil->report_error( "%s", mhdf_message( &status ) );
    return MB_FAILURE;
  }
  qa_list.resize(qa_len);
  for (int i = 0; i < qa_len; i++)
  {
    qa_list[i] = qa[i];
    free( qa[i] );
  }
  free( qa );
  
  rval = iFace->create_meshset( MESHSET_SET, import_set );
  if (MB_SUCCESS != rval)
    return rval;
  
  rval = MB_SUCCESS;
  if (!setSet.range.empty())
    rval = iFace->add_entities( import_set, setSet.range );
  setSet.range.insert( import_set );
  if (MB_SUCCESS != rval)
    return rval;
  
  if (!nodeSet.range.empty())
  {
    rval = iFace->add_entities( import_set, nodeSet.range );
    if (MB_SUCCESS != rval)
      return rval;
  }
  
  std::list<ElemSet>::iterator iter = elemList.begin();
  for ( ; iter != elemList.end(); ++iter )
  {
    if (iter->range.empty())
      continue;
    
    rval = iFace->add_entities( import_set, iter->range );
    if (MB_SUCCESS != rval)
      return rval;
  }
  
  /** FIX ME - how to put QA list on set?? */

  return MB_SUCCESS;
}

  
    
