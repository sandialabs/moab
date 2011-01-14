/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile$

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMOABReader.h"

#include "vtkUnstructuredGrid.h"
#include "vtkObjectFactory.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkExtractCells.h"
#include "vtkFloatArray.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkIntArray.h"
#include "vtkCharArray.h"
#include "vtkPolyData.h"
#include "vtkObjectFactory.h"
#include "vtkUnstructuredGrid.h"
#include "vtkExtractUnstructuredGrid.h"
#include "vtkThreshold.h"
#include "vtkDoubleArray.h"
#include "vtkIntArray.h"
#include "vtkPointData.h"
#include "vtkAlgorithmOutput.h"

#include "moab/Core.hpp"
#include "moab/WriteUtilIface.hpp"
#include "moab/Range.hpp"
#include "MBTagConventions.hpp"
#include "MBParallelConventions.h"
#include "moab/CN.hpp"

#include <sstream>
#include <vector>
#include <string>
#include "assert.h"

using namespace moab;

vtkStandardNewMacro(vtkMOABReader)

class vtkIntArray;

class VTK_IO_EXPORT vtkMOABReaderPrivate : public vtkObject
{
public:
  static vtkMOABReaderPrivate *New();
  vtkTypeMacro(vtkMOABReaderPrivate, vtkObject);

  ErrorCode load_file(const char *file_name, const char *options, EntityHandle &file_set);

  vtkUnstructuredGrid *GetOutput();
  void SetOutput(vtkUnstructuredGrid *ug);

  void file_sets(Range &files);
  
  void file_names(std::vector<std::string> &fnames);

  bool file_loaded(const char *filename);
  
  friend class vtkMOABReader;

protected:

private:
  vtkMOABReaderPrivate();
  ~vtkMOABReaderPrivate();

  vtkMOABReaderPrivate(const vtkMOABReaderPrivate&);  // Not implemented.
  void operator=(const vtkMOABReaderPrivate&);  // Not implemented.

  int RequestData(vtkInformation *vtkNotUsed(request), 
                  vtkInformationVector **vtkNotUsed(inputVector), 
                  vtkInformationVector *outputVector);
  
  ErrorCode construct_mesh(EntityHandle file_set);

  ErrorCode create_points_vertices(EntityHandle file_set, Range &verts);
  
  ErrorCode create_elements(EntityHandle file_set);
  
  ErrorCode construct_filters();

  ErrorCode read_tags(EntityHandle file_set);
  
  ErrorCode read_sparse_tags(EntityHandle file_set);

  ErrorCode read_dense_tags(EntityHandle file_set);

  void add_name(vtkUnstructuredGrid *output, const char *prefix,
                const int id);
  
  ErrorCode get_top_parent_sets(Range &top_sets);
  
  vtkMultiBlockDataSet *get_mbdataset(vtkMultiBlockDataSet *output,
                                      EntityHandle eset, bool create_if_missing = false);
  
  vtkExtractCells *get_ecdataset(vtkMultiBlockDataSet *output,
                                 EntityHandle eset, bool create_if_missing = false);
  
  ErrorCode get_category_name(EntityHandle eset, std::string &cat_name);

  ErrorCode recursive_process_set(EntityHandle eset, vtkMultiBlockDataSet *output);

  ErrorCode process_parent_sets(vtkMultiBlockDataSet *output);

  ErrorCode process_tagged_sets(vtkMultiBlockDataSet *output);
  
  vtkUnstructuredGrid *myUG;

  Interface *mbImpl;
  
  WriteUtilIface *iFace;

  Range fileSets;
                               
  static const int vtk_cell_types[];

  int numPointIds;
  int numCellIds;

  Tag vtkPointTag, vtkCellTag;

  Tag vtkDSTag;
  
  std::vector<std::string> fileNames;

  bool outOfDate;
  
  bool iConstructedMOAB;

  Tag gidTag, gdimTag, partTag, catTag;
};

vtkStandardNewMacro(vtkMOABReaderPrivate);

inline vtkUnstructuredGrid *vtkMOABReaderPrivate::GetOutput() 
{
  return myUG;
}

inline void vtkMOABReaderPrivate::SetOutput(vtkUnstructuredGrid *ug) 
{
  myUG = ug;
}

inline void vtkMOABReaderPrivate::file_sets(Range &fsets) 
{
  fsets.merge(fileSets);
}

inline void vtkMOABReaderPrivate::file_names(std::vector<std::string> &fnames) 
{
  std::copy(fileNames.begin(), fileNames.end(), std::back_inserter(fnames));
}

inline bool vtkMOABReaderPrivate::file_loaded(const char *filename) 
{
  return (std::find(fileNames.begin(), fileNames.end(), std::string(filename)) != fileNames.end());
}

#define MOABMeshErrorMacro(s) std::cerr s << std::endl
using namespace moab;

const int vtkMOABReaderPrivate::vtk_cell_types[] = {
  1, 3, 5, 9, 7, 10, 14, 13, 0, 12, 0, 0, 0};

const bool new_outputs = false;
const bool use_filters = true;

vtkMOABReader::vtkMOABReader()
{
  this->SetNumberOfInputPorts(0);
  this->FileName = NULL;
  masterReader = NULL;
}

vtkMOABReader::~vtkMOABReader()
{
  if (masterReader)
    delete masterReader;
}

void vtkMOABReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//-----------------------------------------------------------------------------
// RequestInformation
int vtkMOABReader::RequestInformation(vtkInformation *vtkNotUsed(request), 
                                      vtkInformationVector **vtkNotUsed(inputVector), 
                                      vtkInformationVector *outputVector)
{
  if (!this->FileName || strlen(this->FileName) == 0)
    {
    vtkErrorMacro("FileName has to be specified!");
    return 0;
    }

    // make a private reader and tell it to read MOAB data
  if (!masterReader) masterReader = vtkMOABReaderPrivate::New();
  EntityHandle file_set = 0;
  moab::ErrorCode rval = masterReader->load_file(FileName, NULL, file_set);
  return (MB_SUCCESS == rval ? 1 : 0);

}

//-----------------------------------------------------------------------------
// RequestData
int vtkMOABReader::RequestData(vtkInformation *request, 
                               vtkInformationVector **inputVector, 
                               vtkInformationVector *outputVector)
{
  return masterReader->RequestData(request, inputVector, outputVector);
  
/*
  int nSteps = 0;
  double *requestedTimeValues = NULL;
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS()))
    {
    requestedTimeValues
        = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEPS());
    nSteps = outInfo->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
    }

  if (nSteps > 0)
    {
    outInfo->Set(vtkDataObject::DATA_TIME_STEPS(), requestedTimeValues, 1);
    this->SetTimeValue(requestedTimeValues[0]);
    }

  if (this->Parent == this)
    {
    output->GetFieldData()->AddArray(this->CasePath);
    if (!this->MakeMetaDataAtTimeStep(false))
      {
      return 0;
      }
    this->CurrentReaderIndex = 0;
    }

  // compute flags
  // internal mesh selection change is detected within each reader
  const bool recreateInternalMesh = (!this->Parent->CacheMesh)
      || this->Parent->DecomposePolyhedra
          != this->Parent->DecomposePolyhedraOld || this->Parent->ReadZones
      != this->Parent->ReadZonesOld || this->Parent->ListTimeStepsByControlDict
      != this->Parent->ListTimeStepsByControlDictOld;
  const bool recreateBoundaryMesh =
      this->Parent->PatchDataArraySelection->GetMTime()
          != this->Parent->PatchSelectionMTimeOld
          || this->Parent->CreateCellToPoint
              != this->Parent->CreateCellToPointOld;
  const bool updateVariables = this->Parent->CellDataArraySelection->GetMTime()
      != this->Parent->CellSelectionMTimeOld
      || this->Parent->PointDataArraySelection->GetMTime()
          != this->Parent->PointSelectionMTimeOld
      || this->Parent->LagrangianDataArraySelection->GetMTime()
          != this->Parent->LagrangianSelectionMTimeOld
      || this->Parent->PositionsIsIn13Format
          != this->Parent->PositionsIsIn13FormatOld
      || this->Parent->AddDimensionsToArrayNames
          != this->Parent->AddDimensionsToArrayNamesOld;

  // create dataset
  int ret = 1;
  vtkMOABReaderPrivate *reader;
  // if the only region is not a subregion, omit being wrapped by a
  // multiblock dataset
  if (this->Readers->GetNumberOfItems() == 1 && (reader = vtkMOABReaderPrivate::SafeDownCast(
          this->Readers->GetItemAsObject(0)))->GetRegionName() == "")
    {
    ret = reader->RequestData(output, recreateInternalMesh,
        recreateBoundaryMesh, updateVariables);
    this->Parent->CurrentReaderIndex++;
    }
  else
    {
    this->Readers->InitTraversal();
    while ((reader
        = vtkMOABReaderPrivate::SafeDownCast(this->Readers->GetNextItemAsObject()))
        != NULL)
      {
      vtkMultiBlockDataSet *subOutput = vtkMultiBlockDataSet::New();
      if (reader->RequestData(subOutput, recreateInternalMesh,
          recreateBoundaryMesh, updateVariables))
        {
        vtkStdString regionName(reader->GetRegionName());
        if (regionName == "")
          {
          regionName = "defaultRegion";
          }
        const int blockI = output->GetNumberOfBlocks();
        output->SetBlock(blockI, subOutput);
        output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), regionName.c_str());
        }
      else
        {
        ret = 0;
        }
      subOutput->Delete();
      this->Parent->CurrentReaderIndex++;
      }
    }

  if (this->Parent == this) // update only if this is the top-level reader
    {
    this->UpdateStatus();
    }
*/
}

//-----------------------------------------------------------------------------
void vtkMOABReader::UpdateProgress(double amount)
{
  this->vtkAlgorithm::UpdateProgress(1.0);
}

vtkMOABReaderPrivate::vtkMOABReaderPrivate() 
        : mbImpl(NULL), myUG(NULL), iFace(NULL), numPointIds(0), numCellIds(0), 
          vtkPointTag(0), vtkCellTag(0), outOfDate(true), iConstructedMOAB(false), gidTag(0), 
          gdimTag(0), partTag(0), catTag(0)
{
  if (!mbImpl) {
    mbImpl = new Core();
    iConstructedMOAB = true;
  }

  mbImpl->query_interface("WriteUtilIface", reinterpret_cast<void**>(&iFace));
  assert(NULL != iFace);

  vtkIdType def_val = -1;
  ErrorCode rval = mbImpl->tag_create("__vtkCellTag", sizeof(vtkIdType), MB_TAG_DENSE, MB_TYPE_INTEGER, 
                                      vtkCellTag, &def_val, true);
  assert(MB_SUCCESS == rval || MB_ALREADY_ALLOCATED == rval);

  rval = mbImpl->tag_create("__vtkPointTag", sizeof(vtkIdType), MB_TAG_DENSE, MB_TYPE_INTEGER, 
                            vtkPointTag, &def_val, true);
  assert(MB_SUCCESS == rval || MB_ALREADY_ALLOCATED == rval);

  vtkDataSet *ds = NULL;
  rval = mbImpl->tag_create("__vtkDataSet", sizeof(vtkDataSet*), MB_TAG_SPARSE, MB_TYPE_OPAQUE,
                            vtkDSTag, &ds, true);
  assert(MB_SUCCESS == rval);
  
  rval = mbImpl->tag_get_handle(GLOBAL_ID_TAG_NAME, gidTag);
  assert(MB_SUCCESS == rval);
      
  rval = mbImpl->tag_get_handle(GEOM_DIMENSION_TAG_NAME, gdimTag);
      
  rval = mbImpl->tag_get_handle(PARALLEL_PARTITION_TAG_NAME, partTag);
      
  rval = mbImpl->tag_get_handle(CATEGORY_TAG_NAME, catTag);
      
}

ErrorCode vtkMOABReaderPrivate::load_file(const char *file_name, const char *options, 
                                          EntityHandle &file_set) 
{
  ErrorCode rval;
  
  rval = mbImpl->create_meshset(MESHSET_SET, file_set);
  if (MB_SUCCESS != rval) return rval;

  fileSets.insert(file_set);
    
  rval = mbImpl->load_file(file_name, &file_set, options);
  if (MB_SUCCESS != rval) return rval;

  outOfDate = true;
  
  fileNames.push_back(std::string(file_name));

  return rval;
}

vtkMOABReaderPrivate::~vtkMOABReaderPrivate()
{
  if (mbImpl && iConstructedMOAB) {
    if (iFace)
      mbImpl->release_interface("WriteUtilIface", reinterpret_cast<void*>(iFace));

    delete mbImpl;
  }
}

ErrorCode vtkMOABReaderPrivate::read_tags(EntityHandle file_set) 
{
  ErrorCode rval = read_dense_tags(file_set);
  if (MB_SUCCESS != rval) return rval;

//  rval = read_sparse_tags(file_set);
  return rval;
}

ErrorCode vtkMOABReaderPrivate::read_dense_tags(EntityHandle file_set) 
{  
    // get all the tags
  std::vector<Tag> tmptags, all_tags;
  ErrorCode rval = mbImpl->tag_get_tags(tmptags);
  if (MB_SUCCESS != rval) return rval;
  
  for (std::vector<Tag>::iterator vit = tmptags.begin(); vit != tmptags.end(); vit++) {
      // skip sparse tags
    TagType ttype;
    rval = mbImpl->tag_get_type(*vit, ttype);
    if (MB_SUCCESS == rval && MB_TAG_DENSE == ttype) all_tags.push_back(*vit);
  }

    // now create field arrays on all 2d and 3d entities
  Range ents2d, ents3d, verts;
  rval = mbImpl->get_entities_by_dimension(file_set, 2, ents2d);
  if (MB_SUCCESS != rval) return rval;

  rval = mbImpl->get_entities_by_dimension(file_set, 3, ents3d);
  if (MB_SUCCESS != rval) return rval;

  rval = mbImpl->get_entities_by_dimension(file_set, 0, verts);
  if (MB_SUCCESS != rval) return rval;

  vtkIdType *vids;
  void *data;
  if (MB_SUCCESS != rval) return rval;
  std::vector<double> tag_dvals;
  std::vector<int> tag_ivals;
  vtkIntArray *int_array;
  vtkDoubleArray *dbl_array;
  int idef, *idata;
  double ddef, *ddata;
  int min, max;
      
  for (std::vector<Tag>::iterator vit = all_tags.begin(); vit != all_tags.end(); vit++) {
    if (*vit == vtkCellTag) continue;
    
      // create a data array
    DataType dtype;
    rval = mbImpl->tag_get_data_type(*vit, dtype);
    if (MB_SUCCESS != rval) continue;
    std::string tag_name;
    bool has_default = false;
    rval = mbImpl->tag_get_name(*vit, tag_name);
    if (MB_SUCCESS != rval) continue;
    if (MB_TYPE_DOUBLE == dtype) {
      dbl_array = vtkDoubleArray::New();
      dbl_array->SetName(tag_name.c_str());
      if (MB_SUCCESS == mbImpl->tag_get_default_value(*vit, &ddef))
        has_default = true;
    }
    else if (MB_TYPE_INTEGER == dtype) {
      int_array = vtkIntArray::New();
      int_array->SetName(tag_name.c_str());
      if (MB_SUCCESS == mbImpl->tag_get_default_value(*vit, &idef))
        has_default = true;
    }

    if (MB_SUCCESS != rval) continue;

    Range::iterator rit, rit2;
    rit = rit2 = ents2d.begin();
    while (rit != ents2d.end()) {
        // get tag iterator for gids
      rval = mbImpl->tag_iterate(vtkCellTag, rit, ents2d.end(), (void*&)vids);
      if (MB_SUCCESS != rval) continue;
      int count = rit - rit2;
      
      rval = mbImpl->tag_iterate(*vit, rit2, ents2d.end(), data);
      if (MB_SUCCESS != rval) continue;
      
      if (MB_TYPE_DOUBLE == dtype) {
        ddata = (double*)data;
        for (int i = 0; i < count; i++) {
          assert(-1 < vids[i] && vids[i] < numCellIds);
          if (!has_default || ddata[i] != ddef)
            dbl_array->InsertValue(vids[i], ddata[i]);
        }
      }
      else if (MB_TYPE_INTEGER == dtype) {
        idata = (int*)data;
        for (int i = 0; i < count; i++) {
          assert(-1 < vids[i] && vids[i] < numCellIds);
          if (!has_default || idata[i] != idef)
            int_array->InsertValue(vids[i], idata[i]);
        }
      }
      
      min = *std::min_element(vids, vids+count);
      max = *std::max_element(vids, vids+count);
      MOABMeshErrorMacro(<< "2d: min = " << min << ", max =  " << max);
    }
    
    rit = rit2 = ents3d.begin();
    while (rit != ents3d.end()) {
        // get tag iterator for vids
      rval = mbImpl->tag_iterate(vtkCellTag, rit, ents3d.end(), (void*&)vids);
      if (MB_SUCCESS != rval) continue;
      int count = rit - rit2;
      
      rval = mbImpl->tag_iterate(*vit, rit2, ents3d.end(), data);
      if (MB_SUCCESS != rval) continue;
      
      if (MB_TYPE_DOUBLE == dtype) {
        ddata = (double*)data;
        for (int i = 0; i < count; i++) {
          assert(-1 < vids[i] && vids[i] < numCellIds);
          if (!has_default || ddata[i] != ddef)
            dbl_array->InsertValue(vids[i], ddata[i]);
        }
      }
      else if (MB_TYPE_INTEGER == dtype) {
        idata = (int*)data;
        for (int i = 0; i < count; i++) {
          assert(-1 < vids[i] && vids[i] < numCellIds);
          if (!has_default || idata[i] != idef)
            int_array->InsertValue(vids[i], idata[i]);
        }
      }
      min = *std::min_element(vids, vids+count);
      max = *std::max_element(vids, vids+count);
      MOABMeshErrorMacro(<< "3d: min = " << min << ", max =  " << max);
    }
    
    rit = rit2 = verts.begin();
    while (rit != verts.end()) {
        // get tag iterator for vids
      rval = mbImpl->tag_iterate(vtkCellTag, rit, verts.end(), (void*&)vids);
      if (MB_SUCCESS != rval) continue;
      int count = rit - rit2;
      
      rval = mbImpl->tag_iterate(*vit, rit2, verts.end(), data);
      if (MB_SUCCESS != rval) continue;
      
      if (MB_TYPE_DOUBLE == dtype) {
        ddata = (double*)data;
        for (int i = 0; i < count; i++) {
          assert(vids[i] >= 0 && vids[i] < numCellIds);
          if (!has_default || ddata[i] != ddef)
            dbl_array->InsertValue(vids[i], ddata[i]);
        }
      }
      else if (MB_TYPE_INTEGER == dtype) {
        idata = (int*)data;
        for (int i = 0; i < count; i++) {
          assert(vids[i] >= 0 && vids[i] < numCellIds);
          if (!has_default || idata[i] != idef)
            int_array->InsertValue(vids[i], idata[i]);
        }
      }
      
      min = *std::min_element(vids, vids+count);
      max = *std::max_element(vids, vids+count);
      MOABMeshErrorMacro(<< "verts: min = " << min << ", max =  " << max);
    }
    
    if (MB_TYPE_DOUBLE == dtype) {
      this->GetOutput()->GetCellData()->AddArray(dbl_array);
      dbl_array->Delete();
      MOABMeshErrorMacro(<< "Read " << dbl_array->GetSize() << " values of dbl tag " << tag_name);
    }
    else if (MB_TYPE_INTEGER == dtype) {
      this->GetOutput()->GetCellData()->AddArray(int_array);
      int_array->Delete();
      MOABMeshErrorMacro(<< "Read " << int_array->GetSize() << " values of int tag " << tag_name);
    }
  }

  return MB_SUCCESS;
}

ErrorCode vtkMOABReaderPrivate::read_sparse_tags(EntityHandle file_set) 
{  
    // get all the tags
  std::vector<Tag> tmptags, all_tags;
  ErrorCode rval = mbImpl->tag_get_tags(tmptags);
  if (MB_SUCCESS != rval) return rval;
  
  for (std::vector<Tag>::iterator vit = tmptags.begin(); vit != tmptags.end(); vit++) {
      // skip dense tags
    TagType ttype;
    DataType dtype;
    rval = mbImpl->tag_get_type(*vit, ttype);
    rval = mbImpl->tag_get_data_type(*vit, dtype);
    if (MB_SUCCESS == rval && MB_TAG_SPARSE == ttype && MB_TYPE_INTEGER == dtype) 
      all_tags.push_back(*vit);
  }

    // now create field arrays on all 2d and 3d entities
  Range sets, ents, verts;
  vtkIntArray *int_array;

  Tag gid_tag, gdim_tag;
  std::vector<vtkIdType> vids;
  for (std::vector<Tag>::iterator vit = all_tags.begin(); vit != all_tags.end(); vit++) {
    if (*vit == vtkCellTag) continue;

      // if this is a geometry tag, loop
    int lmax = (*vit == gdim_tag ? 3 : 0);
    static int lvals[] = {0, 1, 2, 3};
    static const char *lnames[] = {"GeomVertex", "GeomCurve", "GeomSurface", "GeomVolume"};
    
    for (int l = 0; l <= lmax; l++) {
      sets.clear();
      int *lval = lvals+l;
      rval = mbImpl->get_entities_by_type_and_tag(file_set, MBENTITYSET, &(*vit), 
                                                  (const void* const*)(lmax ? &lval : NULL), 1, sets);
      if (MB_SUCCESS != rval || sets.empty()) continue;
      
        // create a data array
      std::string tag_name;
      bool has_default = false;
      if (lmax) tag_name = std::string(lnames[l]);
      else {
        rval = mbImpl->tag_get_name(*vit, tag_name);
        if (MB_SUCCESS != rval) continue;
      }
      if (MB_SUCCESS != rval) continue;
      int_array = vtkIntArray::New();
      int_array->SetName(tag_name.c_str());
      bool had_ents = false;
      
        // loop over sets then entities
      for (Range::iterator rit = sets.begin(); rit != sets.end(); rit++) {
          // get the tag value
        int this_val;
        rval = mbImpl->tag_get_data((lmax ? gid_tag : *vit), &(*rit), 1, &this_val);
        if (MB_SUCCESS != rval) continue;

          // get the entities, and their vtk ids
        ents.clear();
        for (int d = 0; d <= 3; d++) {
          rval = mbImpl->get_entities_by_dimension(*rit, d, ents, true);
          if (MB_SUCCESS != rval) continue;
        }
        if (ents.empty()) continue;
        vids.resize(ents.size());
        rval = mbImpl->tag_get_data(vtkCellTag, ents, &vids[0]);
        if (MB_SUCCESS != rval || ents.empty()) continue;

        for (unsigned int e = 0; e < vids.size(); e++) {
          assert(-1 != vids[e]);
          int_array->InsertValue(vids[e], this_val);
        }

        had_ents = true;
      }

        // add the data array to the output
      if (had_ents) this->GetOutput()->GetCellData()->AddArray(int_array);
      int_array->Delete();
    }
  }

  return MB_SUCCESS;
}

ErrorCode vtkMOABReaderPrivate::construct_mesh(EntityHandle file_set) 
{
    // construct the vtk representation of the mesh
  
    // get all the hexes and quads
  Range all_elems;
  ErrorCode result = MB_SUCCESS, tmp_result;
  for (int dim = 0; dim <= 3; dim++) 
  {
    tmp_result = mbImpl->get_entities_by_dimension(file_set, dim, all_elems);
    if (tmp_result != MB_SUCCESS) result = tmp_result;
  }
  if (MB_SUCCESS != result)
    {
    MOABMeshErrorMacro( << "Failure getting hexes from mesh. " );
    return result;
    }

  MOABMeshErrorMacro(<< "Read " << all_elems.size() << " entities from MOAB.");

    // create the elements
  int success = this->create_elements(file_set);
  if (MB_SUCCESS != result)
    {
    MOABMeshErrorMacro( << "Problem filling in quad data. " );
    return result;
    }

  return MB_SUCCESS;
  
}

ErrorCode vtkMOABReaderPrivate::create_points_vertices(EntityHandle file_set, Range &verts) 
{
    // get the global id tag
  ErrorCode result;

  result = mbImpl->get_entities_by_type(file_set, MBVERTEX, verts);
  if (MB_SUCCESS != result)
  {
    MOABMeshErrorMacro( << "Couldn't gather vertices. " );
    return result;
  }

  MOABMeshErrorMacro(<< "Gathered " << verts.size() << " vertices from MOAB.");
  
    // assign ids to the vertices
  std::vector<vtkIdType> vids(verts.size());
  for (unsigned int i = 0; i < verts.size(); i++)
    vids[i] = numPointIds++;

  result = mbImpl->tag_set_data(vtkPointTag, verts, &vids[0]);
  if (MB_SUCCESS != result)
  {
    MOABMeshErrorMacro( << "Couldn't set ids on vertices. " );
    return result;
  }
  
    // allocate and fill in coordinate arrays
  std::vector<double*> coords(3);
  coords[0] = new double[verts.size()];
  coords[1] = new double[verts.size()];
  coords[2] = new double[verts.size()];
  result = iFace->get_node_coords(3, verts.size(), verts,
                                  0, 0, coords);
  if (MB_SUCCESS != result)
  {
    MOABMeshErrorMacro( << "Couldn't get nodal coordinates. " );
    return result;
  }

    // put these data into a point array
  vtkPoints *points = vtkPoints::New();
  int dum;
  points->SetNumberOfPoints(verts.size());
  assert(MB_SUCCESS == result);
  unsigned int i = numPointIds - verts.size();
  for (Range::const_iterator rit = verts.begin(); rit != verts.end(); rit++, i++)
  {
    points->SetPoint(vids[i], coords[0][i], coords[1][i], coords[2][i]);
  }
  myUG->SetPoints(points);

    // create point cells for these points
  for (unsigned int i = 0; i < verts.size(); i++) {
    vtkIdType vid = vids[i];
    vids[i] = myUG->InsertNextCell(vtk_cell_types[0], 1, &vid);
    assert(numCellIds == vids[i]);
    numCellIds++;
  }
  result = mbImpl->tag_set_data(vtkCellTag, verts, &vids[0]);
  if (MB_SUCCESS != result)
  {
    MOABMeshErrorMacro( << "Couldn't set ids on vertex cells. " );
    return result;
  }

  points->Delete();

  return MB_SUCCESS;
}

ErrorCode vtkMOABReaderPrivate::create_elements(EntityHandle file_set)
{
    // get the vertices
  Range verts;
  ErrorCode result;

    // create points/vertices in vtk database
  result = create_points_vertices(file_set, verts);
  if (MB_SUCCESS != result)
  {
    MOABMeshErrorMacro( << "Couldn't create points/vertices. " );
    return result;
  }

  MOABMeshErrorMacro(<< "After create_points_vertices: ug has " << myUG->GetNumberOfPoints()
                     << " points, " << myUG->GetNumberOfCells() << " cells.");
  
    // for the remaining elements, add them individually
  vtkIdType ids[CN::MAX_NODES_PER_ELEMENT];
  const EntityHandle *connect;
  int num_connect;
  bool first = true;

  for (EntityType this_type = MBEDGE; this_type != MBENTITYSET; this_type++) {

      // don't try to represent elements vtk doesn't understand
    if (vtk_cell_types[this_type] == 0) continue;
    
    Range elems;
    result = mbImpl->get_entities_by_type(file_set, this_type, elems);
    if (MB_SUCCESS != result)
    {
      MOABMeshErrorMacro( << "Couldn't get elements. " );
      return result;
    }

    std::vector<vtkIdType> eids(elems.size());
    result = mbImpl->tag_get_data(vtkCellTag, elems, &eids[0]);
    if (MB_SUCCESS != result)
    {
      MOABMeshErrorMacro( << "Couldn't get elements vtkCellTag. " );
      return result;
    }
    
    int e = 0;
    bool changed = false;
    for (Range::iterator rit = elems.begin(); rit != elems.end(); rit++, e++) {
      if (-1 != eids[e]) continue;
      
      changed = true;
      
        // get the connectivity of these elements
      result = mbImpl->get_connectivity(*rit, connect, num_connect, true);
      if (MB_SUCCESS != result)
      {
        MOABMeshErrorMacro( << "Couldn't get element connectivity. " );
        return result;
      }

        // get the id tag for these vertices
      result = mbImpl->tag_get_data(vtkCellTag, connect, num_connect, ids);
      if (MB_SUCCESS != result)
      {
        MOABMeshErrorMacro( << "Couldn't get vertex ids for element. " );
        return result;
      }

        // ok, now insert this cell
      eids[e] = myUG->InsertNextCell(vtk_cell_types[this_type], num_connect, ids);
      assert(eids[e] == numCellIds);
      numCellIds++;
    }

    if (changed) {
      result = mbImpl->tag_set_data(vtkCellTag, elems, &eids[0]);
      if (MB_SUCCESS != result)
      {
        MOABMeshErrorMacro( << "Couldn't save element ids. " );
        return result;
      }
    }
  }
  
  MOABMeshErrorMacro(<< "After creating cells: ug has " << myUG->GetNumberOfPoints()
                     << " points, " << myUG->GetNumberOfCells() << " cells.");
  
  return MB_SUCCESS;
}

moab::ErrorCode vtkMOABReaderPrivate::construct_filters() 
{
    // apply threshold and type filters to the output to get multiple actors
    // corresponding to dual surfaces and curves, then group the dual actors
    // together using a group filter

/*
    // first, get the non-dual mesh
  vtkExtractUnstructuredGrid *primal = vtkExtractUnstructuredGrid::New();
  primal->SetInput(this->GetOutput());
  primal->SetCellMinimum(0);
  primal->SetCellMaximum(this->MaxPrimalId);

    // set merging on so points aren't duplicated
  primal->SetMerging(1);

    // now do dual surfaces; do threshold-based extraction for now
  MBTag vtkCellTag;
  MBErrorCode result = mbImpl->tag_get_handle(GLOBAL_ID_TAG_NAME, vtkCellTag);
  assert(MB_SUCCESS == result && 0 != vtkCellTag);
  
  int ds_id;
  for (ds_id = 0; ds_id < this->NumberOfDualSurfaces; ds_id++) {
    vtkThreshold *ds_filter = vtkThreshold::New();
    ds_filter->SelectInputScalars(DUAL_SURF_ATTRIBUTE_NAME);
    ds_filter->SetAttributeModeToUseCellData();
    ds_filter->ThresholdBetween(((double)ds_id-0.5), ((double)ds_id+0.5));
    ds_filter->SetInput(ug);
    this->add_name(ds_filter->GetOutput(), "dual_surf_", ds_id);
  }
  
    // same for dual curves
  int dc_id;
  for (dc_id = 0; dc_id < this->NumberOfDualCurves; dc_id++) {
    vtkThreshold *dc_filter = vtkThreshold::New();
    dc_filter->SelectInputScalars(DUAL_CURVE_ATTRIBUTE_NAME);
    dc_filter->SetAttributeModeToUseCellData();
    dc_filter->ThresholdBetween(((double)dc_id-0.5), ((double)dc_id+0.5));
    dc_filter->SetInput(ug);
    this->add_name(dc_filter->GetOutput(), "dual_curve_", dc_id);
  }

    // lastly, get the dual vertices and put those in a group
    // first, get the non-dual mesh
  vtkExtractUnstructuredGrid *dual_verts = vtkExtractUnstructuredGrid::New();
  dual_verts->SetCellMinimum(this->DualVertexIdOffset);
  dual_verts->SetCellMaximum(this->DualVertexIdOffset+this->NumberOfDualVertices-1);
  this->add_name(dual_verts->GetOutput(), "dual_verts", 0);
*/
  return MB_SUCCESS;
}

void vtkMOABReaderPrivate::add_name(vtkUnstructuredGrid *output, const char *prefix,
                           const int id) 
{
  vtkCharArray* nmArray =  vtkCharArray::New();
  nmArray->SetName("Name");
  vtkstd::ostringstream name;
  name << prefix << id << "\0";
  nmArray->SetNumberOfTuples(static_cast<vtkIdType>(name.str().length()));
  char* copy = nmArray->GetPointer(0);
  memcpy(copy, name.str().c_str(), name.str().length());
  output->GetFieldData()->AddArray(nmArray);
  nmArray->Delete();
}

int vtkMOABReaderPrivate::RequestData(vtkInformation *vtkNotUsed(request), 
                                      vtkInformationVector **vtkNotUsed(inputVector), 
                                      vtkInformationVector *outputVector) 
{
  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkMultiBlockDataSet
      *output =
          vtkMultiBlockDataSet::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  const int blockI = output->GetNumberOfBlocks();
  myUG = vtkUnstructuredGrid::New();
  output->SetBlock(blockI, myUG);
  output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), "Mesh");
  
    // get the data set & allocate an initial chunk of data
  myUG->Allocate();
  
  moab::ErrorCode rval  = construct_mesh(*fileSets.begin());
  if (MB_SUCCESS != rval)
  {
    MOABMeshErrorMacro( << "Failed to construct mesh");
    return rval;
  }
  MOABMeshErrorMacro(<<"Constructed mesh...");

  if (use_filters)
    rval = construct_filters();
  if (MB_SUCCESS != rval)
  {
    MOABMeshErrorMacro( << "Failed to construct filters ");
    return MB_FAILURE;
  }
  MOABMeshErrorMacro(<<"Filters constructed...");
  
    // get all dense tags
  rval = read_tags(*fileSets.begin());
  MOABMeshErrorMacro(<<"Tags read...");

  MOABMeshErrorMacro(<< "After Update: ug has " << myUG->GetNumberOfPoints()
                     << " points, " << myUG->GetNumberOfCells() << " cells.");

    // process parent, tagged sets
  rval = process_parent_sets(output);
  if (MB_SUCCESS != rval) return 0;
  
  rval = process_tagged_sets(output);
  if (MB_SUCCESS != rval) return 0;
  
  return (rval == MB_SUCCESS);
}

ErrorCode vtkMOABReaderPrivate::process_parent_sets(vtkMultiBlockDataSet *output) 
{
  Range par_sets;
  ErrorCode rval = get_top_parent_sets(par_sets);
  if (MB_SUCCESS != rval || par_sets.empty()) return rval;
  
    // ok, we have parent/child hierarchy; make the top-level item, then descend recursively
  vtkMultiBlockDataSet *ds = vtkMultiBlockDataSet::New();
  const int blockI = output->GetNumberOfBlocks();
  output->SetBlock(blockI, ds);
  output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), "Parent Sets");

  for (Range::iterator rit = par_sets.begin(); rit != par_sets.end(); rit++) {
    rval = recursive_process_set(*rit, ds);
    if (MB_SUCCESS != rval) return rval;
  }

  ds->Delete();
  
  return MB_SUCCESS;
}

ErrorCode vtkMOABReaderPrivate::process_tagged_sets(vtkMultiBlockDataSet *output) 
{
  
    // ok, we have parent/child hierarchy; make the top-level item, then descend recursively
  vtkMultiBlockDataSet *ds = vtkMultiBlockDataSet::New();
  int blockI = output->GetNumberOfBlocks();
  output->SetBlock(blockI, ds);
  output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), "Tagged Sets");

  // get a list of tags
  std::vector<Tag> tag_handles;
  ErrorCode rval = mbImpl->tag_get_tags(tag_handles);
  if (MB_SUCCESS != rval) return rval;

  for (std::vector<Tag>::iterator tag_it = tag_handles.begin(); tag_it != tag_handles.end();
       tag_it++) {
    
    std::string tag_name;
    rval = mbImpl->tag_get_name(*tag_it, tag_name);
    if (MB_SUCCESS != rval) continue;

    // don't display tags with "__" prefix
    if (0 == strncmp(tag_name.c_str(), "__", 2))
      continue;
    
    // get all the sets which contain this tag
    Range tag_sets;
    rval = mbImpl->get_entities_by_type_and_tag(
        0, MBENTITYSET, &(*tag_it), NULL, 1, tag_sets, Interface::UNION);
    if (MB_SUCCESS != rval || tag_sets.empty()) continue;

      // non-empty; make a dataset for this
    vtkMultiBlockDataSet *tmb = vtkMultiBlockDataSet::New();
    blockI = ds->GetNumberOfBlocks();
    ds->SetBlock(blockI, tmb);
    ds->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), tag_name.c_str());

    for (Range::iterator rit = tag_sets.begin(); rit != tag_sets.end(); rit++) {
      rval = recursive_process_set(*rit, tmb);
      if (MB_SUCCESS != rval) return rval;
    }

    tmb->Delete();
  }

  ds->Delete();
  
  return MB_SUCCESS;
}

ErrorCode vtkMOABReaderPrivate::recursive_process_set(EntityHandle eset, vtkMultiBlockDataSet *output)  
{
    // get all entities in the set first
  Range ents;
  ErrorCode rval = mbImpl->get_entities_by_handle(eset, ents, true);
  if (MB_SUCCESS != rval) return rval;
  
    // if the set has children, make it a multiblock set, otherwise a standard UG
  vtkMultiBlockDataSet *mb = NULL;
  vtkExtractCells *ec = NULL;
  vtkUnstructuredGrid *ug = NULL;
  Range children;
  rval = mbImpl->get_child_meshsets(eset, children);
  assert(MB_SUCCESS == rval);
  if (!children.empty()) {
    mb = get_mbdataset(output, eset, true);
    output = mb;
  }

  if (!ents.empty()) {

      // get a new extractcells filter
    ec = get_ecdataset(output, eset, true);
    assert(ec);
  
      // fill it with the entities
    vtkIdList *ids = vtkIdList::New();
    ids->SetNumberOfIds(ents.size());
    rval = mbImpl->tag_get_data(vtkCellTag, ents, ids->GetPointer(0));
    if (MB_SUCCESS != rval) return rval;
  
    ec->SetCellList(ids);
    ec->Update();
    ids->Delete();
  }
  
  for (Range::iterator rit = children.begin(); rit != children.end(); rit++) 
    rval = recursive_process_set(*rit, mb);
  
  return rval;
}

ErrorCode vtkMOABReaderPrivate::get_category_name(EntityHandle eset, std::string &cat_name) 
{
    // look for a category name
  char *tmp_name[CATEGORY_TAG_SIZE];
  ErrorCode rval;
  int id = -1;
  ostrstream os;

    // first get the gid, if any
  rval = mbImpl->tag_get_data(gidTag, &eset, 1, &id);
  if (MB_SUCCESS != rval) id = 0;
  
  if (catTag) {
    rval = mbImpl->tag_get_data(catTag, &eset, 1, tmp_name);
    if (MB_SUCCESS == rval) {
      os << tmp_name << id;
      cat_name = os.str();
      return MB_SUCCESS;
    }
  }
  
    // geom id
  static const char *lnames[] = {"Vertex", "Curve", "Surface", "Volume"};
  if (gidTag) {
    int gdim;
    rval = mbImpl->tag_get_data(gdimTag, &eset, 1, &gdim);
    if (MB_SUCCESS == rval) {
      assert(0 <= gdim && gdim <= 3);
      os << lnames[gdim] << id;
      cat_name = os.str();
      return MB_SUCCESS;
    }
  }
  
    // partition
  if (partTag) {
    int part;
    rval = mbImpl->tag_get_data(partTag, &eset, 1, &part);
    if (MB_SUCCESS == rval) {
      os << "Part" << id;
      cat_name = os.str();
      return MB_SUCCESS;
    }
  }
  
    // nothing
  os << "Set" << id;
  cat_name = os.str();
  return MB_SUCCESS;
}
    
vtkMultiBlockDataSet *vtkMOABReaderPrivate::get_mbdataset(vtkMultiBlockDataSet *output,
                                                          EntityHandle eset, bool create_if_missing) 
{
    // get a vtkMultiBlockDataSet for the set, or NULL if it isn't one
  vtkDataSet *tmp_val = NULL;
  ErrorCode rval = mbImpl->tag_get_data(vtkDSTag, &eset, 1, &tmp_val);
  if (MB_SUCCESS != rval && !create_if_missing) return NULL;
  
  vtkMultiBlockDataSet *ds_val;
  if (!tmp_val && create_if_missing) {
    ds_val = vtkMultiBlockDataSet::New();
    rval = mbImpl->tag_set_data(vtkDSTag, &eset, 1, &ds_val);
    assert(MB_SUCCESS == rval);
    
    const int blockI = output->GetNumberOfBlocks();
    output->SetBlock(blockI, ds_val);

    std::string set_name;
    rval = get_category_name(eset, set_name);
    if (MB_SUCCESS == rval)
      output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), set_name.c_str());
  }
  else
    ds_val = vtkMultiBlockDataSet::SafeDownCast(tmp_val);

  return ds_val;
}

vtkExtractCells *vtkMOABReaderPrivate::get_ecdataset(vtkMultiBlockDataSet *output,
                                                     EntityHandle eset, bool create_if_missing) 
{
    // get a vtkUG for the set, or NULL if it isn't one
  vtkDataSet *tmp_val = NULL;
  ErrorCode rval = mbImpl->tag_get_data(vtkDSTag, &eset, 1, &tmp_val);
  if (MB_SUCCESS != rval && !create_if_missing) return NULL;

  vtkExtractCells *ec_val = NULL;
  vtkUnstructuredGrid *ug_val = NULL;
  vtkMultiBlockDataSet *ds_val = NULL;
  
  if (tmp_val) {
    ds_val = vtkMultiBlockDataSet::SafeDownCast(tmp_val);
    if (ds_val) {
      ug_val = vtkUnstructuredGrid::SafeDownCast(ds_val->GetBlock(0));
      if (!ug_val)
          // had a multiblock dataset, but no ug yet; if we make a ug later, make sure it has
          // the multiblock dataset as its parent
        output = ds_val;
    }
    else
      ug_val = vtkUnstructuredGrid::SafeDownCast(tmp_val);
  }

  if (!ug_val && create_if_missing) {
    ec_val = vtkExtractCells::New();
    ec_val->SetInput(myUG);
    ug_val = ec_val->GetOutput();

    const int blockI = output->GetNumberOfBlocks();
    output->SetBlock(blockI, ug_val);

    std::string set_name;
    rval = get_category_name(eset, set_name);
    if (MB_SUCCESS == rval)
      output->GetMetaData(blockI)->Set(vtkCompositeDataSet::NAME(), set_name.c_str());
  }
  else if (ug_val) {
    ec_val = vtkExtractCells::SafeDownCast(ug_val->GetProducerPort()->GetProducer());
  }
  
  return ec_val;
}

ErrorCode vtkMOABReaderPrivate::get_top_parent_sets(Range &top_sets) 
{
    // get top parent sets, which are those who aren't children of others but have
    // some children
  top_sets.clear();
  Range dum_sets;
  ErrorCode rval = mbImpl->get_entities_by_type(0, MBENTITYSET, dum_sets);
  if (MB_SUCCESS != rval || dum_sets.empty()) return rval;

  ErrorCode tmp_rval;
  int num_children, num_parents;
  for (Range::iterator rit = dum_sets.begin(); rit != dum_sets.end(); rit++) {
    tmp_rval = mbImpl->num_child_meshsets(*rit, &num_children);
    if (MB_SUCCESS != tmp_rval) rval = tmp_rval;
    tmp_rval = mbImpl->num_parent_meshsets(*rit, &num_parents);
    if (MB_SUCCESS != tmp_rval) rval = tmp_rval;
    if (num_parents == 0 && num_children > 0) top_sets.insert(*rit);
  }
  
  return rval;
}

