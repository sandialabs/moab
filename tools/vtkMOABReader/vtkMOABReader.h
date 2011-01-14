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
// .NAME vtkMOABReader - read vtk unstructured grid data file
// .SECTION Description
// vtkMOABReader is a source object that reads ASCII or binary 
// unstructured grid data files in vtk format. (see text for format details).
// The output of this reader is a single vtkUnstructuredGrid data object.
// The superclass of this class, vtkDataReader, provides many methods for
// controlling the reading of the data file, see vtkDataReader for more
// information.
// .SECTION Caveats
// Binary files written on one system may not be readable on other systems.
// .SECTION See Also
// vtkUnstructuredGrid vtkDataReader

#ifndef __vtkMOABReader_h
#define __vtkMOABReader_h

#include "vtkMultiBlockDataSetAlgorithm.h"

class vtkInformation;
class vtkInformationVector;
class vtkMOABReaderPrivate;

#include <map>

class VTK_IO_EXPORT vtkMOABReader : public vtkMultiBlockDataSetAlgorithm
{
public:
  static vtkMOABReader *New();
  vtkTypeMacro(vtkMOABReader,vtkMultiBlockDataSetAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Specify file name of the Exodus file.
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  void UpdateProgress(double amount);
  
protected:
  vtkMOABReader();
  ~vtkMOABReader();

  int RequestInformation(vtkInformation *vtkNotUsed(request), 
                         vtkInformationVector **vtkNotUsed(inputVector), 
                         vtkInformationVector *outputVector);

  int RequestData(vtkInformation *vtkNotUsed(request), 
                  vtkInformationVector **vtkNotUsed(inputVector), 
                  vtkInformationVector *outputVector);
  
private:
  vtkMOABReader(const vtkMOABReader&);  // Not implemented.
  void operator=(const vtkMOABReader&);  // Not implemented.

  char *FileName;

  vtkMOABReaderPrivate *masterReader;
};

#endif


