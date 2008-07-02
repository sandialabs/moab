#ifndef MB_MESHOUTPUTFUNCTOR_HPP
#define MB_MESHOUTPUTFUNCTOR_HPP

#include "MBTypes.h"
#include "MBEntityRefiner.hpp"
#include "MBParallelComm.hpp"

#include <iostream>
#include <set>
#include <map>
#include <iterator>
#include <algorithm>

template< int _n >
class MBSplitVertexIndex
{
public:
  MBSplitVertexIndex() { }
  MBSplitVertexIndex( const MBEntityHandle* src )
    { for ( int i = 0; i < _n; ++ i ) this->handles[i] = src[i]; std::sort( this->handles, this->handles + _n ); }
  MBSplitVertexIndex( const MBSplitVertexIndex<_n>& src )
    { for ( int i = 0; i < _n; ++ i ) this->handles[i] = src.handles[i]; }
  MBSplitVertexIndex& operator = ( const MBSplitVertexIndex<_n>& src )
    { for ( int i = 0; i < _n; ++ i ) this->handles[i] = src.handles[i]; return *this; }

  bool operator < ( const MBSplitVertexIndex<_n>& other ) const
    {
    for ( int i = 0; i < _n; ++ i )
      if ( this->handles[i] < other.handles[i] )
        return true;
      else if ( this->handles[i] > other.handles[i] )
        return false;
    return false;
    }

  MBEntityHandle handles[_n];
};

class MBSplitVerticesBase
{
public:
  MBSplitVerticesBase( MBRefinerTagManager* tag_mgr )
    {
    this->tag_manager = tag_mgr;
    this->mesh_in  = tag_mgr->get_input_mesh();
    this->mesh_out = tag_mgr->get_output_mesh();
    this->shared_procs_val.resize( MAX_SHARING_PROCS );
    MBParallelComm* ipcomm = MBParallelComm::get_pcomm( this->mesh_in, 0 );
    this->rank = ipcomm ? ipcomm->proc_config().proc_rank() : 0;
    }
  virtual ~MBSplitVerticesBase() { }
  virtual bool find_or_create( const MBEntityHandle* split_src, const double* coords, MBEntityHandle& vert_handle ) = 0;

  /// Prepare to compute the processes on which a new split-vertex will live.
  void begin_vertex_procs()
    {
    this->first_vertex = true;
    }

  /// Call this for each existing corner vertex used to define a split-vertex.
  void add_vertex_procs( MBEntityHandle vert_in )
    {
    std::set<int> current_shared_procs;
    if ( ! this->first_vertex )
      {
      current_shared_procs = this->common_shared_procs;
      }
    this->common_shared_procs.clear();
    int stat;
    bool got = false;
    stat = this->mesh_in->tag_get_data(
      this->tag_manager->shared_proc(), &vert_in, 1, &this->shared_procs_val[0] );
    std::cout << "sstat: " << stat;
    if ( stat == MB_SUCCESS && this->shared_procs_val[0] != -1 )
      {
      got = true;
      std::cout << " s" << this->rank << " s" << this->shared_procs_val[0] << " | ";
      this->shared_procs_val.resize( 1 );
      }
    stat = this->mesh_in->tag_get_data(
      this->tag_manager->shared_procs(), &vert_in, 1, &this->shared_procs_val[0] );
    std::cout << "mstat: " << stat;
    if ( stat == MB_SUCCESS && this->shared_procs_val[0] != -1 )
      {
      got = true;
      int i;
      for ( i = 0; i < MAX_SHARING_PROCS && this->shared_procs_val[i] != -1; ++ i )
        std::cout << " m" << this->shared_procs_val[i];
      this->shared_procs_val.resize( i );
      std::cout << " | ";
      }
    if ( got )
      {
      if ( this->first_vertex )
        {
        std::copy(
          this->shared_procs_val.begin(), this->shared_procs_val.end(),
          std::insert_iterator< std::set<int> >( this->common_shared_procs, this->common_shared_procs.begin() ) );
        this->first_vertex = false;
        }
      else
        {
        std::set_intersection(
          this->shared_procs_val.begin(), this->shared_procs_val.end(),
          current_shared_procs.begin(), current_shared_procs.end(),
          std::insert_iterator< std::set<int> >( this->common_shared_procs, this->common_shared_procs.begin() ) );
        }
      }
    else
      {
      std::cout << " not shared | ";
      }
    }

  /// Call this once after all the add_vertex_procs() calls for a split-vertex to prepare queues for the second stage MPI send. 
  void end_vertex_procs()
    {
    std::cout << "    Common procs ";
    std::copy(
      this->common_shared_procs.begin(), this->common_shared_procs.end(),
      std::ostream_iterator<int>( std::cout, " " ) );
    std::cout << " " << this->rank;
    std::cout << "\n";
    // FIXME: Here is where we add the vertex to the appropriate queues.
    }

  MBInterface* mesh_in; // Input mesh. Needed to determine tag values on split_src verts
  MBInterface* mesh_out; // Output mesh. Needed for new vertex set in vert_handle
  MBRefinerTagManager* tag_manager;
  std::vector<int> shared_procs_val; // Used to hold procs sharing an input vert.
  std::set<int> common_shared_procs; // Holds intersection of several shared_procs_vals.
  int rank; // This process' rank.
  bool first_vertex; // True just after begin_vertex_procs() is called.
};

template< int _n >
class MBSplitVertices : public std::map<MBSplitVertexIndex<_n>,MBEntityHandle>, public MBSplitVerticesBase
{
public:
  typedef std::map<MBSplitVertexIndex<_n>,MBEntityHandle> MapType;
  typedef typename std::map<MBSplitVertexIndex<_n>,MBEntityHandle>::iterator MapIteratorType;

  MBSplitVertices( MBRefinerTagManager* tag_mgr )
    : MBSplitVerticesBase( tag_mgr )
    {
    this->shared_procs_val.resize( _n * MAX_SHARING_PROCS );
    }
  virtual ~MBSplitVertices() { }
  virtual bool find_or_create( const MBEntityHandle* split_src, const double* coords, MBEntityHandle& vert_handle )
    {
    MBSplitVertexIndex<_n> key( split_src );
    MapIteratorType it = this->find( key );
    if ( it == this->end() )
      {
      if ( this->mesh_out->create_vertex( coords, vert_handle ) != MB_SUCCESS )
        {
        return false;
        }
      (*this)[key] = vert_handle;
      std::cout << "New vertex " << vert_handle << " shared with ";
      this->begin_vertex_procs();
      for ( int i = 0; i < _n; ++ i )
        {
        this->add_vertex_procs( split_src[i] );
        }
      std::cout << "\n";
      this->end_vertex_procs();
      // Decide whether local process owns new vert.
      // Add to the appropriate queues for transmitting handles.
      return true;
      }
    vert_handle = it->second;
    return false;
    }
};

class MBMeshOutputFunctor : public MBEntityRefinerOutputFunctor
{
public:
  MBInterface* mesh_in;
  MBInterface* mesh_out;
  bool input_is_output;
  std::vector<MBSplitVerticesBase*> split_vertices;
  std::vector<MBEntityHandle> elem_vert;
  MBRefinerTagManager* tag_manager;
  MBEntityHandle destination_set;

  MBMeshOutputFunctor( MBRefinerTagManager* tag_mgr )
    {
    this->mesh_in  = tag_mgr->get_input_mesh();
    this->mesh_out = tag_mgr->get_output_mesh();
    this->input_is_output = ( this->mesh_in == this->mesh_out );
    this->tag_manager = tag_mgr;
    this->destination_set = 0; // don't place output entities in a set by default.

    this->split_vertices.resize( 4 );
    this->split_vertices[0] = 0; // Vertices (0-faces) cannot be split
    this->split_vertices[1] = new MBSplitVertices<1>( this->tag_manager );
    this->split_vertices[2] = new MBSplitVertices<2>( this->tag_manager );
    this->split_vertices[3] = new MBSplitVertices<3>( this->tag_manager );
    }

  ~MBMeshOutputFunctor()
    {
    for ( int i = 0; i < 4; ++ i )
      delete this->split_vertices[i];
    }

  void print_vert_crud( MBEntityHandle vout, int nvhash, MBEntityHandle* vhash, const double* vcoords, const void* vtags )
    {
    std::cout << "+ {";
    for ( int i = 0; i < nvhash; ++ i )
      std::cout << " " << vhash[i];
    std::cout << " } -> " << vout << " ";

    std::cout << "[ " << vcoords[0];
    for ( int i = 1; i < 6; ++i )
      std::cout << ", " << vcoords[i];
    std::cout << " ] ";

#if 0
    double* x = (double*)vtags;
    int* m = (int*)( (char*)vtags + 2 * sizeof( double ) );
    std::cout << "< " << x[0]
              << ", " << x[1];
    for ( int i = 0; i < 4; ++i )
      std::cout << ", " << m[i];
#endif // 0

    std::cout << " >\n";
    }

  void assign_tags( MBEntityHandle vhandle, const void* vtags )
    {
    if ( ! vhandle )
      return; // Ignore bad vertices

    int num_tags = this->tag_manager->get_number_of_vertex_tags();
    MBTag tag_handle;
    int tag_offset;
    for ( int i = 0; i < num_tags; ++i )
      {
      this->tag_manager->get_output_vertex_tag( i, tag_handle, tag_offset );
      this->mesh_out->tag_set_data( tag_handle, &vhandle, 1, vtags );
      }
    }

  virtual MBEntityHandle operator () ( MBEntityHandle vhash, const double* vcoords, const void* vtags )
    {
    if ( this->input_is_output )
      { // Don't copy the original vertex!
      this->print_vert_crud( vhash, 1, &vhash, vcoords, vtags );
      return vhash;
      }
    MBEntityHandle vertex_handle;
    if ( this->mesh_out->create_vertex( vcoords + 3, vertex_handle ) != MB_SUCCESS )
      {
      std::cerr << "Could not insert mid-edge vertex!\n";
      }
    this->assign_tags( vertex_handle, vtags );
    this->print_vert_crud( vertex_handle, 1, &vhash, vcoords, vtags );
    return vertex_handle;
    }

  virtual MBEntityHandle operator () ( int nvhash, MBEntityHandle* vhash, const double* vcoords, const void* vtags )
    {
    MBEntityHandle vertex_handle;
    if ( nvhash == 1 )
      {
      vertex_handle = (*this)( *vhash, vcoords, vtags );
      }
    else if ( nvhash < 4 )
      {
      bool newly_created = this->split_vertices[nvhash]->find_or_create( vhash, vcoords, vertex_handle );
      if ( newly_created )
        {
        this->assign_tags( vertex_handle, vtags );
        }
      if ( ! vertex_handle )
        {
        std::cerr << "Could not insert mid-edge vertex!\n";
        }
      this->print_vert_crud( vertex_handle, nvhash, vhash, vcoords, vtags );
      }
    else
      {
      vertex_handle = 0;
      std::cerr << "Not handling splits on faces with " << nvhash << " corners yet.\n";
      }
    return vertex_handle;
    }

  virtual void operator () ( MBEntityHandle h )
    {
    std::cout << h << " ";
    this->elem_vert.push_back( h );
    }

  virtual void operator () ( MBEntityType etyp )
    {
    MBEntityHandle elem_handle;
    if ( this->mesh_out->create_element( etyp, &this->elem_vert[0], this->elem_vert.size(), elem_handle ) == MB_FAILURE )
      {
      std::cerr << " *** ";
      }
    this->elem_vert.clear();
    std::cout << "---------> " << elem_handle << " ( " << etyp << " )\n\n";
    }
};

#endif // MB_MESHOUTPUTFUNCTOR_HPP
