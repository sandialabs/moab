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


#ifndef MB_INTERNALS_HPP
#define MB_INTERNALS_HPP

#ifdef WIN32
#pragma warning(disable : 4786)
#endif

#ifndef IS_BUILDING_MB
#error "MBInternals.hpp isn't supposed to be included into an application"
#endif

#include <vector>

#include "MBInterface.hpp"

/*! Define MBEntityHandle for both 32 bit and 64 bit systems.
 *  The decision to use 64 bit handles must be made at compile time.
 *  \bug we should probably have an Int64 typedef
 *
 *  MBEntityHandle format:
 *  0xXYYYYYYY  (assuming a 32-bit handle.  Top 4 bits reserved on a 64 bit system)
 *  X - reserved for entity type.  This system can only handle 15 different types
 *  Y - Entity id space.  Max id is over 200M
 *
 *  Note that for specialized databases (such as all hex) 16 bits are not
 *  required for the entity type and the id space can be increased to over 2B.
 */

#define MB_HANDLE_SHIFT_WIDTH (8*sizeof(MBEntityHandle)-4)
#define MB_HANDLE_MASK ((MBEntityHandle)0xF << MB_HANDLE_SHIFT_WIDTH)


#define MB_START_ID 1              //!< All entity id's currently start at 1
#define MB_END_ID ~MB_HANDLE_MASK //!< Last id is the complement of the MASK

//! define non-inline versions of these functions for debugging
extern MBEntityHandle ifh(MBEntityHandle handle);
extern MBEntityType tfh(MBEntityHandle handle);

//! Given a type and an id create a handle.  
inline MBEntityHandle CREATE_HANDLE(int type, MBEntityHandle id, int& err) 
{
  err = 0; //< Assume that there is a real error value defined somewhere

  if (id > ~MB_HANDLE_MASK || type > MBMAXTYPE)
  {
    err = 1;   //< Assume that there is a real error value defined somewhere
    return 1;  //<You've got to return something.  What do you return?
  }
  
  MBEntityHandle handle = type;
  handle = (handle << MB_HANDLE_SHIFT_WIDTH) + id;

  return handle;
}

//! Get the entity id out of the handle.
inline unsigned long ID_FROM_HANDLE (MBEntityHandle handle)
{
  return (handle & ~MB_HANDLE_MASK);
}

//! Get the type out of the handle.
inline MBEntityType TYPE_FROM_HANDLE(MBEntityHandle handle) 
{
  return static_cast<MBEntityType> (
      ((handle) & MB_HANDLE_MASK) >> MB_HANDLE_SHIFT_WIDTH);

}

//! base id of tag handles
typedef unsigned int MBTagId;

/* MBTag format
 * 0xXXZZZZZZ  ( 32 bits total )
 * Z - reserved for internal sub-tag id
 * X - reserved for internal properties & lookup speed
 */
#define TAG_ID_MASK              0x00FFFFFF
#define TAG_PROP_MASK            0xFF000000

inline MBTagId ID_FROM_TAG_HANDLE(MBTag tag_handle) 
{
  return static_cast<MBTagId>( (reinterpret_cast<long>(tag_handle)) & TAG_ID_MASK );
}

inline MBTag TAG_HANDLE_FROM_ID(MBTagId tag_id, MBTagType prop) 
{
  return reinterpret_cast<MBTag>(tag_id | prop);
}

//! define non-inline versions of these functions for debugging
extern int ifth(MBTag handle);

//! define non-inline versions of these functions for debugging
extern int ifth(MBTag handle);
extern MBEntityType tfth(MBTag handle);

#endif




#ifdef UNIT_TEST
#include <iostream>

//! Sample code on a 32-bit system.
int main()
{
  //! define a sample handle
  unsigned int handle = 0x10000010;

  //! display the handle in hex
  std::cout << "Handle: " << std::hex << handle << std::endl;

  //! Get the type and id out of the handle
  std::cout << "Type: " << TYPE_FROM_HANDLE (handle)  << std::endl;
  std::cout << "Id  : " << ID_FROM_HANDLE (handle)  << std::endl;

  //! Create a new handle and make sure it is the same as the first handle
  int err;
  handle = CREATE_HANDLE(1, 16, err);
  if (!err)
    std::cout << "Handle: " << std::hex << handle << std::endl;

  //! define a sample handle
  handle = 0x1FFFFFFF;

  //! display the handle in hex
  std::cout << std::endl;
  std::cout << "Handle: " << std::hex << handle << std::endl;

  //! Get the type and id out of the handle
  std::cout << "Type: " << TYPE_FROM_HANDLE (handle)  << std::endl;
  std::cout << "Id  : " << ID_FROM_HANDLE (handle)  << std::endl;

  //! This handle represents the maximum possible ID in 32 bits.
  std::cout << "Max Id  : " << std::dec << ID_FROM_HANDLE (handle)  << std::endl;

  //! Create a new handle and make sure it is the same as the first handle
  handle = CREATE_HANDLE(1,16, err);
  if (!err)
    std::cout << "Handle: " << std::hex << handle << std::endl;

  for (int i = MeshVertex; i <= 16; i++)
  {
    handle = CREATE_HANDLE(i, 0x0FFFFFFF, err);
    if (!err)
    {
      std::cout << "Handle: " << std::hex << handle << std::endl;
      std::cout << "Type: " << std::dec << TYPE_FROM_HANDLE (handle)  << std::endl;
      std::cout << "Id  : " << ID_FROM_HANDLE (handle)  << std::endl;
    }
    else
      std::cout << "Out of ID space" << std::endl;
  }

  return 0;
}

#endif
