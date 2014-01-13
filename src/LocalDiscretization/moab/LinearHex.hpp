#ifndef LINEAR_HEX_HPP
#define LINEAR_HEX_HPP
  /**\brief Shape function space for trilinear hexahedron, obtained by a pushforward of the canonical linear (affine) functions. */

#include "moab/ElemEvaluator.hpp"

namespace moab 
{
    
class LinearHex 
{
public:
    /** \brief Forward-evaluation of field at parametric coordinates */
  static ErrorCode evalFcn(const double *params, const double *field, const int ndim, const int num_tuples, 
                           double *work, double *result);
        
    /** \brief Reverse-evaluation of parametric coordinates at physical space position */
  static ErrorCode reverseEvalFcn(EvalFcn eval, JacobianFcn jacob, InsideFcn ins, 
                                  const double *posn, const double *verts, const int nverts, const int ndim,
                                  const double iter_tol, const double inside_tol, double *work, 
                                  double *params, int *is_inside);
        
    /** \brief Evaluate the jacobian at a specified parametric position */
  static ErrorCode jacobianFcn(const double *params, const double *verts, const int nverts, const int ndim, 
                               double *work, double *result);
        
    /** \brief Forward-evaluation of field at parametric coordinates */
  static ErrorCode integrateFcn(const double *field, const double *verts, const int nverts, const int ndim, const int num_tuples, 
                                double *work, double *result);

        /** \brief Function that returns whether or not the parameters are inside the natural space of the element */
  static int insideFcn(const double *params, const int ndim, const double tol);
  
  static EvalSet eval_set() 
      {
        return EvalSet(evalFcn, reverseEvalFcn, jacobianFcn, integrateFcn, (InitFcn)NULL, insideFcn);
      }
      
  static bool compatible(EntityType tp, int numv, EvalSet &eset) 
      {
        if (tp == MBHEX && numv == 8) {
          eset = eval_set();
          return true;
        }
        else return false;
      }
  
protected:
    /* Preimages of the vertices -- "canonical vertices" -- are known as "corners". */
  static const double corner[8][3];
  static const double gauss[1][2];
  static const unsigned int corner_count = 8;
  static const unsigned int gauss_count  = 1;
      
};// class LinearHex

} // namespace moab

#endif
