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

#ifndef EXOII_DEFINES
#define EXOII_DEFINES

// CJS  -- ExoIIInterface replaces this
#error "don't include"

enum ExoIIElementType 
{
  EXOII_SPHERE = 0,
  EXOII_SPRING,
  EXOII_BAR, EXOII_BAR2, EXOII_BAR3,
  EXOII_BEAM, EXOII_BEAM2, EXOII_BEAM3,
  EXOII_TRUSS, EXOII_TRUSS2, EXOII_TRUSS3,
  EXOII_TRI, EXOII_TRI3, EXOII_TRI6, EXOII_TRI7,
  EXOII_QUAD, EXOII_QUAD4, EXOII_QUAD5, EXOII_QUAD8, EXOII_QUAD9,
  EXOII_SHEL, EXOII_SHELL4, EXOII_SHELL8, EXOII_SHELL9,
  EXOII_TETRA, EXOII_TETRA4, EXOII_TETRA8, EXOII_TETRA10, EXOII_TETRA14,
  EXOII_PYRAMID, EXOII_PYRAMID5, EXOII_PYRAMID8, EXOII_PYRAMID13, EXOII_PYRAMID18,
  EXOII_WEDGE,
  EXOII_KNIFE,
  EXOII_HEX, EXOII_HEX8, EXOII_HEX9, EXOII_HEX20, EXOII_HEX27,
  EXOII_HEXSHELL,
  EXOII_MAX_ELEM_TYPE
};

#endif
