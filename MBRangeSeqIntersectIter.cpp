/*
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

/**\file MBRangeSeqIntersectIter.cpp
 *\author Jason Kraftcheck (kraftche@cae.wisc.edu)
 *\date 2006-08-11
 */

#include "MBRangeSeqIntersectIter.hpp"
#include "EntitySequenceManager.hpp"
#include "EntitySequence.hpp"

MBErrorCode MBRangeSeqIntersectIter::init( MBRange::const_iterator start,
                                           MBRange::const_iterator end )
{
  mSequence = 0;
  rangeIter = start;

    // special case : nothing to iterate over
  if (start == end) {
    mStartHandle = mEndHandle = mLastHandle = 0;
    return MB_FAILURE;
  }

    // normal case
  mStartHandle = *start;
  --end;
  mLastHandle = *end;
  mEndHandle = (*rangeIter).second;
  if (mEndHandle > mLastHandle)
    mEndHandle = mLastHandle;

#if MB_RANGE_SEQ_INTERSECT_ITER_STATS
  MBErrorCode result = update_entity_sequence();
  update_stats(mEndHandle - mStartHandle + 1);
  return result;
#else
  return update_entity_sequence();
#endif
}
  

MBErrorCode MBRangeSeqIntersectIter::step()
{
    // If at end, return MB_FAILURE
  if (is_at_end())
    return MB_FAILURE; 
    // If the last block was at the end of the rangeIter pair,
    // then advance the iterator and set the next block
  else if (mEndHandle == (*rangeIter).second) {
    ++rangeIter;
    mStartHandle = (*rangeIter).first;
  }
    // Otherwise start with next entity in the pair
  else {
    mStartHandle = mEndHandle + 1;
  }
    // Always take the remaining entities in the rangeIter pair.
    // will trim up the end of the range in update_entity_sequence().
  mEndHandle = (*rangeIter).second;
  if (mEndHandle > mLastHandle)
    mEndHandle = mLastHandle;
  
    // Now trim up the range (decrease mEndHandle) as necessary
    // for the corresponding EntitySquence
#if MB_RANGE_SEQ_INTERSECT_ITER_STATS
  MBErrorCode result = update_entity_sequence();
  update_stats(mEndHandle - mStartHandle + 1);
  return result;
#else
  return update_entity_sequence();
#endif
}

MBErrorCode MBRangeSeqIntersectIter::update_entity_sequence()
{
    // mStartHandle to mEndHandle is a subset of the MBRange.
    // Update sequence data as necessary and trim that subset
    // (reduce mEndHandle) for the current EntitySequence.
  
    // Need to update the sequence pointer?
  if (!mSequence || mStartHandle > mSequence->get_end_handle()) {
  
      // Check that the mStartHandle is not a mesh set.
      // We don't care if mEndHandle is (yet) because the end
      // will be trimmed to the end of the EntitySequence
      // containing mStartHandle.
    if (TYPE_FROM_HANDLE(mStartHandle) >= MBENTITYSET)
      return MB_TYPE_OUT_OF_RANGE;

    if (MB_SUCCESS != mSequenceManager->find( mStartHandle, mSequence ))
      return find_invalid_range();

    freeIndex = mSequence->get_next_free_index( -1 );
    if (freeIndex < 0) 
      freeIndex = mSequence->number_allocated();
  }
  
    // Find first hole in sequence after mStartHandle
  int start_index = mStartHandle - mSequence->get_start_handle();
  while (start_index > freeIndex) {
    freeIndex = mSequence->get_next_free_index( freeIndex );
    if (freeIndex == -1)
      freeIndex = mSequence->number_allocated();
  }
  if (start_index == freeIndex) 
    return find_deleted_range();
    
    // if mEndHandle is past end of sequence or block of used
    // handles within sequence, shorten it.
  int end_index = mEndHandle - mSequence->get_start_handle();
  if(end_index >= freeIndex)
    mEndHandle = mSequence->get_start_handle() + (freeIndex-1);
    
  return MB_SUCCESS;
}
 
typedef std::map<MBEntityHandle,MBEntitySequence*> SeqMap;

MBErrorCode MBRangeSeqIntersectIter::find_invalid_range()
{
  assert(!mSequence);

    // no more entities in current range
  if (mStartHandle == mEndHandle)
    return MB_ENTITY_NOT_FOUND;
    
    // Find the next MBEntitySequence
  MBEntityType type = TYPE_FROM_HANDLE(mStartHandle);
  const SeqMap* map = mSequenceManager->entity_map( type );
  SeqMap::const_iterator iter = map->upper_bound( mStartHandle );
    // If no next sequence of the same type
  if (iter == map->end()) {
      // If end type not the same as start type, split on type
    if (type != TYPE_FROM_HANDLE( mEndHandle )) {
      int junk;
      mEndHandle = CREATE_HANDLE( type, MB_END_ID, junk );
    }
  }
    // otherwise invalid range ends at min(mEndHandle, sequence start handle - 1)
  else if (iter->second->get_start_handle() <= mEndHandle) {
    mEndHandle = iter->second->get_start_handle()-1;
  }
  
  return MB_ENTITY_NOT_FOUND;
}

MBErrorCode MBRangeSeqIntersectIter::find_deleted_range()
{ 
    // If we're here, then its because freeIndex == start_index
  assert (mSequence);
  assert (mSequence->get_start_handle() + freeIndex >= mStartHandle);

    // Find the last deleted entity before mEndHandle
  int end_index = mEndHandle - mSequence->get_start_handle();
  while (freeIndex < end_index) {
    int index = mSequence->get_next_free_index( freeIndex );
      // If there was a break in the span of deleted entities before
      // mEndHandle, then set mEndHandle to the end of the span of
      // deleted entities and stop.  In the case where we've reached
      // the end of the list (index < 0) then freeIndex will be the
      // last index in the sequence.
    if (index < 0 || index - freeIndex > 1) {
      mEndHandle = mSequence->get_start_handle() + freeIndex;
      break;
    }
    freeIndex = index;
  }

  return MB_ENTITY_NOT_FOUND;
}
        
#if MB_RANGE_SEQ_INTERSECT_ITER_STATS
double MBRangeSeqIntersectIter::doubleNumCalls = 0;
double MBRangeSeqIntersectIter::doubleEntCount = 0;
unsigned long MBRangeSeqIntersectIter::intNumCalls = 0;
unsigned long MBRangeSeqIntersectIter::intEntCount = 0;

void MBRangeSeqIntersectIter::update_stats( unsigned long num_ents )
{
  if (std::numeric_limits<unsigned long>::max() == intNumCalls) {
    doubleNumCalls += intNumCalls;
    intNumCalls = 0;
  }
  ++intNumCalls;
  
  if (std::numeric_limits<unsigned long>::max() - intEntCount > num_ents) {
    doubleNumCalls += intEntCount;
    intEntCount = num_ents;
  }
  else {
    intEntCount += num_ents;
  }
}
#endif

