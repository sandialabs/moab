#include "moab/Solvers.hpp"
#include <iostream>
#include <assert.h>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

namespace moab {

  /* This class implements the lowest level solvers required by polynomial fitting for high-order reconstruction.
   * An underlying assumption of the matrices passed are that they are given in a column-major form. So,
   * the first mrows values of V is the first column, and so on. This assumption is made because most of the
   *  operations are column-based in the current scenario.
   * */

  unsigned int Solvers::nchoosek(unsigned int n, unsigned int k){
    if(k>n){
      return 0;
    }
    unsigned long long ans=1;
    if(k>(n>>1)){
      k = n-k;
    }
    for(unsigned int i=1;i<=k;++i){
      ans *= n--;
      ans /= i;
      if(ans>std::numeric_limits<unsigned int>::max()){
        return 0;
      }
    }
    return ans;
  }

  unsigned int Solvers::compute_numcols_vander_multivar(unsigned int kvars,unsigned int degree){
    unsigned int mcols=0;
    for(unsigned int i=0;i<=degree;++i){
      unsigned int temp = nchoosek(kvars-1+i,kvars-1);
      if(!temp){
        std::cout << "overflow to compute nchoosek n= " << kvars-1+i << " k= " << kvars-1 << std::endl;
        return 0;
      }
      mcols += temp;
    }
    return mcols;
  }

  void Solvers::gen_multivar_monomial_basis(const int kvars,const double* vars, const int degree, std::vector<double>& basis){
    unsigned int len = compute_numcols_vander_multivar(kvars,degree);
    basis.reserve(len-basis.capacity()+basis.size());
    size_t iend = basis.size(),istr = basis.size();
    basis.push_back(1); ++iend;
    if(!degree){
      return;
    }
    std::vector<size_t> varspos(kvars);
    //degree 1
    for(int ivar=0;ivar<kvars;++ivar){
      basis.push_back(vars[ivar]);
      varspos[ivar] = iend++;
    }
    //degree 2 to degree
    for(int ideg=2;ideg<=degree;++ideg){
      size_t preend = iend;
      for(int ivar=0;ivar<kvars;++ivar){
        size_t varpreend = iend;
        for(size_t ilast=varspos[ivar];ilast<preend;++ilast){
          basis.push_back(vars[ivar]*basis[ilast]); ++iend;
        }
        varspos[ivar] = varpreend;
      }
    }
    assert(len==iend-istr);
  }

  void Solvers::gen_vander_multivar(const int mrows,const int kvars, const double* us, const int degree, std::vector<double>& V){
    unsigned int ncols = compute_numcols_vander_multivar(kvars,degree);
    V.reserve(mrows*ncols-V.capacity()+V.size());
    size_t istr=V.size(),icol=0;
    //add ones, V is stored in an single array, elements placed in columnwise order
    for(int irow=0;irow<mrows;++irow){
      V.push_back(1);
    }
    ++icol;
    if(!degree){
      return;
    }
    std::vector<size_t> varspos(kvars);
    //degree 1
    for(int ivar=0;ivar<kvars;++ivar){
      for(int irow=0;irow<mrows;++irow){
        V.push_back(us[irow*kvars+ivar]);//us stored in row-wise
      }
      varspos[ivar] = icol++;
    }
    //from 2 to degree
    for(int ideg=2;ideg<=degree;++ideg){
      size_t preendcol = icol;
      for(int ivar=0;ivar<kvars;++ivar){
        size_t varpreend = icol;
        for(size_t ilast=varspos[ivar];ilast<preendcol;++ilast){
          for(int irow=0;irow<mrows;++irow){
            V.push_back(us[irow*kvars+ivar]*V[istr+irow+ilast*mrows]);
          }
          ++icol;
        }
        varspos[ivar] = varpreend;
      }
    }
    assert(icol==ncols);
  }

  void Solvers::rescale_matrix(int mrows, int ncols, double *V, double *ts)
  {
    //This function rescales the input matrix using the norm of each column.
    double *v = new double[mrows];
    for (int i=0; i< ncols; i++)
      {
        for (int j=0; j<mrows; j++)
          v[j] = V[mrows*i+j];

        //Compute norm of the column vector
        double w = vec_2norm(mrows,v);

        if (fabs(w)==0)
          ts[i] = 1;
        else
          {
            ts[i] = w;
            for (int j=0; j<mrows; j++)
              V[mrows*i+j] = V[mrows*i+j]/ts[i];
          }
      }
    delete [] v;
  }

  void Solvers::compute_qtransposeB(int mrows, int ncols, const double *Q, int bncols, double *bs)
  {
    for (int k=0; k<ncols; k++)
      {
        for (int j=0; j<bncols; j++)
          {
            double t2 = 0;
            for (int i=k; i<mrows; i++)
              t2 += Q[mrows*k+i]*bs[mrows*j+i];
            t2 = t2 + t2;

            for (int i=k; i<mrows; i++)
              bs[mrows*j+i] -= t2*Q[mrows*k+i];
          }
      }
  }

  void Solvers::qr_polyfit_safeguarded( const int mrows, const int ncols, double *V, double *D, int *rank)
  {
    double tol = 1e-8;
    *rank = ncols;
    double *v = new double[mrows];

    for (int k=0; k<ncols; k++)
      {
        int nv = mrows-k;

        for (int j=0; j<nv; j++)
          v[j] = V[mrows*k + (j+k)];

        double t2=0;

        for (int j=0; j<nv; j++)
          t2 = t2 + v[j]*v[j];

        double t = sqrt(t2);
        double vnrm = 0;

        if (v[0] >=0)
          {
            vnrm = sqrt(2*(t2+v[0]*t));
            v[0] = v[0]+t;
          }
        else
          {
            vnrm = sqrt(2*(t2-v[0]*t));
            v[0] = v[0]-t;
          }

        if (vnrm>0)
          {
            for (int j=0; j<nv; j++)
              v[j] = v[j]/vnrm;
          }

        for(int j=k; j<ncols; j++)
          {
            t2 = 0;
            for (int i=0; i<nv; i++)
              t2 = t2 + v[i]*V[mrows*j+(i+k)];

            t2 = t2+t2;

            for (int i=0; i<nv; i++)
                V[mrows*j+(i+k)] = V[mrows*j+(i+k)] - t2*v[i];
          }

        D[k] = V[mrows*k+k];

        for (int i=0; i<nv; i++)
            V[mrows*k+(i+k)] = v[i];

        if ((fabs(D[k])) < tol && (*rank == ncols))
          {
            *rank = k;
            break;
          }
      }

    delete [] v;
  }

  void Solvers::backsolve(int mrows, int ncols, double *R, int bncols, double *bs, double *ws)
  {
   /* std::cout.precision(16);
    std::cout<<"Before backsolve  "<<std::endl;
    std::cout<<" V = "<<std::endl;
    for (int k=0; k< ncols; k++){
        for (int j=0; j<mrows; ++j){
            std::cout<<R[mrows*k+j]<<std::endl;
          }
        std::cout<<std::endl;
      }
    std::cout<<std::endl;

    //std::cout<<"#pnts = "<<mrows<<std::endl;
    std::cout<<"bs = "<<std::endl;
    std::cout<<std::endl;
    for (int k=0; k< bncols; k++){
        for (int j=0; j<mrows; ++j){
            std::cout<<"  "<<bs[mrows*k+j]<<std::endl;
          }
      }
    std::cout<<std::endl;*/


    for (int k=0; k< bncols; k++)
      {
        for (int j=ncols-1; j>=0; j--)
          {
            for (int i=j+1; i<ncols; ++i)
              bs[mrows*k+j] = bs[mrows*k+j] - R[mrows*i+j]*bs[mrows*k+i];

            assert(R[mrows*j+j] != 0);

            bs[mrows*k+j] = bs[mrows*k+j]/R[mrows*j+j];

          //  std::cout<<"bs["<<j<<"] = "<<bs[mrows*k+j]<<std::endl;
          }
      }

    for (int k=0; k< bncols; k++){
        for (int j=0; j<ncols; ++j)
          bs[mrows*k+j] = bs[mrows*k+j]/ws[j];
      }
  }

  void Solvers::backsolve_polyfit_safeguarded(int dim, int degree, int mrows, int ncols, double *R, int bncols, double *bs, const double *ws, int *degree_out)
  {
    /*std::cout.precision(12);
    std::cout<<"Before backsolve  "<<std::endl;
    std::cout<<" V = "<<std::endl;
    for (int k=0; k< ncols; k++){
        for (int j=0; j<mrows; ++j){
            std::cout<<R[mrows*k+j]<<std::endl;
          }
        std::cout<<std::endl;
      }
    std::cout<<std::endl;

    //std::cout<<"#pnts = "<<mrows<<std::endl;
    std::cout<<"bs = "<<std::endl;
    std::cout<<std::endl;
    for (int k=0; k< bncols; k++){
        for (int j=0; j<mrows; ++j){
            std::cout<<"  "<<bs[mrows*k+j]<<std::endl;
        }
    }
    std::cout<<std::endl;*/
    //std::cout<<" ] "<<std::endl;

   /* std::cout<<"Input ws = [ ";
    for (int k=0; k< ncols; k++){
            std::cout<<ws[k]<<", ";
          }
    std::cout<<" ] "<<std::endl;*/

    std::cout << "R: " << R << "size: [" << mrows << "," << ncols << "]" << std::endl;
    std::cout << "bs: " << bs << "size: [" << mrows << "," << bncols << "]" << std::endl;
    std::cout << "ws: " << ws << "size: [" << ncols << "," << 1 << "]" << std::endl;
    std::cout << "degree_out: " << degree_out << std::endl;

    int deg, numcols;

    for (int k=0; k< bncols; k++)
      {
        deg = degree;
        /* I think we should consider interp = true/false -Xinglin*/
        if (dim==1)
          numcols = deg+1;
        else if (dim==2)
          numcols = (deg+2)*(deg+1)/2;

        /*ERROR, assert is disabled*/
        std::cout << "numcols: " << numcols << ", ncols: " << ncols << std::endl;
        assert(numcols <=ncols);

        //double *bs_bak = new double[numcols];
        std::vector<double> bs_bak(numcols);

        if (deg >= 2)
          {
            for (int i=0; i< numcols; i++){
                //bs_bak[i] = bs[mrows*k+i];
                assert(mrows*k+i < mrows*bncols);
                bs_bak.at(i) = bs[mrows*k+i];
                //std::cout<<"bs_bak["<<i<<"] = "<<bs_bak[i]<<std::endl;
              }
          }

        while (deg>=1 )
          {
            int cend = numcols-1;
            bool downgrade = false;

            for (int d = deg; d>=0 ; d--)
              {
                int cstart;
                if (dim==1){
                  cstart = d ;
                }else if (dim==2){
                  cstart = ((d+1)*d)/2 ;
                  //cstart = ((d*(d+1))>>1)-interp;
                }

               // std::cout<<"cstart = "<<cstart<<", cend = "<<cend<<std::endl;

                //Solve for  bs
                for (int j=cend; j>= cstart; j--)
                  {
                    assert(mrows*k+j < mrows*bncols);
                    for (int i=j+1; i<numcols; ++i)
                      {
                        assert(mrows*k+i < mrows*bncols);
                        assert(mrows*i+j < mrows*ncols); // check R
                        bs[mrows*k+j] = bs[mrows*k+j] - R[mrows*i+j]*bs[mrows*k+i];
                      }
                    assert(mrows*j+j < mrows*ncols); // check R
                    bs[mrows*k+j] = bs[mrows*k+j]/R[mrows*j+j];
                    //std::cout<<"bs["<<j<<"] = "<<bs[mrows*k+j]<<std::endl;
                  }

                //Checking for change in the coefficient
                if (d >= 2 && d < deg)
                  {
                    double tol;

                    if (dim == 1)
                      {
                        tol = 1e-06;
                        assert(mrows*cstart+cstart < mrows*ncols); // check R
                        double tb = bs_bak.at(cstart)/R[mrows*cstart+cstart];
                        assert(mrows*k+cstart < mrows*bncols);
                        if (fabs(bs[mrows*k+cstart]-tb) > (1+tol)*fabs(tb))
                          {
                            downgrade = true;
                            break;
                          }
                      }

                    else if (dim == 2)
                      {
                        tol = 0.05;

                  //      std::cout<<"cend = "<<cend<<", cstart = "<<cstart<<std::endl;

                        //double *tb = new double[cend-cstart+1];
                        std::vector<double> tb(cend-cstart+1);
                        for (int j=0; j<=(cend-cstart); j++)
                          {
                            tb.at(j) = bs_bak.at(cstart+j);
                          //  std::cout<<"tb["<<j<<"] = "<<tb[j]<<std::endl;
                          }

                        for (int j=cend; j>= cstart; j--)
                          {
                            int jind = j -cstart;

                         //   std::cout<<"j = "<<j<<", jind = "<<jind<<std::endl;

                            for (int i=j+1; i<=cend; ++i)
                              {
                                assert(mrows*i+j < mrows*ncols); // check R
                                tb.at(jind) = tb.at(jind) - R[mrows*i+j]*tb.at(i-cstart);
                              }
                            assert(mrows*j+j < mrows*ncols); // check R
                            tb.at(jind) = tb.at(jind)/R[mrows*j+j];
                            assert(mrows*k+j < mrows*bncols);
                            double err = fabs(bs[mrows*k+j] - tb.at(jind));

                         //   std::cout<<"fabs(tb[jind])="<<fabs(tb[jind])<<", err = "<<err<<std::endl;

                            if ((err > tol) && (err >= (1+tol)*fabs(tb.at(jind))))
                              {
                                downgrade = true;
                            //    std::cout<<"downgraded"<<std::endl;
                                break;
                              }
                          }

                        //delete [] tb;

                        if (downgrade)
                          break;
                      }
                  }

                cend = cstart -1;
              }

            if (!downgrade)
              break;
            else
              {
                deg = deg - 1;
                if (dim == 1)
                  numcols = deg+1;
                else if (dim == 2)
                  numcols = (deg+2)*(deg+1)/2;

               for (int i=0; i<numcols; i++) {
                 assert(mrows*k+i < mrows*bncols);
                 bs[mrows*k+i] = bs_bak.at(i);
               }
            }
          }
        assert(k < bncols);
        degree_out[k] = deg;

        //std::cout<<"BACKSOLVE_SAFEGUARDED solution bs = [ ";
        std::cout << "numcols: " << numcols << std::endl;
        for (int i=0; i<numcols; i++)
          {
            /*ERROR*/
            assert(mrows*k+i < mrows*bncols);
            assert(i < ncols);
            std::cout << "ws[" << i << "]: " << ws[i] << "\tAddress: " << ws+i << std::endl; 
            bs[mrows*k+i] = bs[mrows*k+i]/ws[i];
        //    std::cout<<bs[mrows*k+i]<<", ";
          }

     //   std::cout<<" ] "<<std::endl;

        for (int i=numcols; i<mrows; i++) {
          assert(mrows*k+i < mrows*bncols);
          bs[mrows*k+i] = 0;
        }

        //delete [] bs_bak;
      }

  }

  void Solvers::vec_dotprod(const int len, const double* a, const double* b, double* c)
  {
    if(!a||!b||!c){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return;
    }
    for(int i=0;i<len;++i){
        c[i] = a[i]*b[i];
      }

    }

  void Solvers::vec_scalarprod(const int len, const double* a, const double c, double *b)
  {
    if(!a||!b){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return;
    }
    for(int i=0;i<len;++i){
         b[i] = c*a[i];
      }
  }

  void Solvers::vec_crossprod(const double a[3], const double b[3], double (&c)[3])
  {
    c[0] =a[1]*b[2]-a[2]*b[1];
    c[1] =a[2]*b[0]-a[0]*b[2];
    c[2] =a[0]*b[1]-a[1]*b[0];
  }

  double Solvers::vec_innerprod(const int len, const double* a, const double* b)
  {
    if(!a||!b){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return 0;
    }
    double ans=0;
    for(int i=0;i<len;++i){
        ans += a[i]*b[i];
    }
    return ans;
  }

  double Solvers::vec_2norm(const int len, const double* a)
  {
    if(!a){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return 0;
    }
    double w=0, s=0;
    for (int k=0; k<len; k++)
      w = std::max(w, fabs(a[k]));

    if(w==0){
        return 0;
    }else{
        for (int k=0; k<len; k++){
          s += (a[k]/w)*(a[k]/w);
        }
        s=w*sqrt(s);
    }
    return s;
  }

  double Solvers::vec_normalize(const int len, const double* a, double* b)
  {
    if(!a||!b){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return 0;
    }
    double nrm=0,mx=0;
    for(int i=0;i<len;++i){
        mx = std::max(fabs(a[i]),mx);
    }
    if(mx==0){
      for(int i=0;i<len;++i){
        b[i] = 0;
      }
      return 0;
    }
    for(int i=0;i<len;++i){
        nrm += (a[i]/mx)*(a[i]/mx);
    }
    nrm = mx*sqrt(nrm);
    if(nrm==0){
        return nrm;
    }
    for(int i=0;i<len;++i){
        b[i] = a[i]/nrm;
    }
    return nrm;
  }

  double Solvers::vec_distance(const int len, const double* a, const double*b){
    double res=0;
    for(int i=0;i<len;++i){
      res += (a[i]-b[i])*(a[i]-b[i]);
    }
    return sqrt(res);
  }


  void Solvers::vec_projoff(const int len, const double* a, const double* b, double* c)
  {
    if(!a||!b||!c){
      std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
      return;
    }
    //c = a-<a,b>b/<b,b>;
    double bnrm = vec_2norm(len,b);
    if (bnrm==0){
        for(int i=0;i<len;++i){
          c[i] = a[i];
        }
        return; 
    }
    double innerp = vec_innerprod(len,a,b)/bnrm;

    if(innerp==0){
      if(c!=a){
        for(int i=0;i<len;++i){
          c[i] = a[i];
        }
      }
      return;
    }

    for(int i=0;i<len;++i){
        c[i] = a[i]-innerp*b[i]/bnrm;
    }
  }

    void Solvers::vec_linear_operation(const int len, const double mu, const double* a, const double psi, const double* b, double* c)
    {
      if(!a||!b||!c){
        std::cerr << __FILE__ << ":" << __LINE__ << "\nNULL Pointer" << std::endl;
        return;
      }
      for(int i=0;i<len;++i){
          c[i] = mu*a[i]+psi*b[i];
      }
    }

    void Solvers::get_tri_natural_coords(const int dim, const double* cornercoords, const int npts, const double* currcoords, double* naturalcoords){
      assert(dim==2||dim==3);
      double a=0,b=0,d=0,tol=1e-12;
      for(int i=0;i<dim;++i){
        a += (cornercoords[dim+i]-cornercoords[i])*(cornercoords[dim+i]-cornercoords[i]);
        b += (cornercoords[dim+i]-cornercoords[i])*(cornercoords[2*dim+i]-cornercoords[i]);
        d += (cornercoords[2*dim+i]-cornercoords[i])*(cornercoords[2*dim+i]-cornercoords[i]);
      }
      double det = a*d-b*b; assert(det>0);
      for(int ipt=0;ipt<npts;++ipt){
        double e=0,f=0;
        for(int i=0;i<dim;++i){
          e += (cornercoords[dim+i]-cornercoords[i])*(currcoords[ipt*dim+i]-cornercoords[i]);
          f += (cornercoords[2*dim+i]-cornercoords[i])*(currcoords[ipt*dim+i]-cornercoords[i]);
        }
        naturalcoords[ipt*3+1] = (e*d-b*f)/det; naturalcoords[ipt*3+2] = (a*f-b*e)/det; naturalcoords[ipt*3] = 1-naturalcoords[ipt*3+1]-naturalcoords[ipt*3+2];
        if(naturalcoords[ipt*3]<-tol||naturalcoords[ipt*3+1]<-tol||naturalcoords[ipt*3+2]<-tol){
          std::cout << "Corners: \n";
          std::cout << cornercoords[0] << "\t" << cornercoords[1] << "\t" << cornercoords[3] << std::endl; 
          std::cout << cornercoords[3] << "\t" << cornercoords[4] << "\t" << cornercoords[5] << std::endl; 
          std::cout << cornercoords[6] << "\t" << cornercoords[7] << "\t" << cornercoords[8] << std::endl;
          std::cout << "Candidate: \n";
          std::cout << currcoords[ipt*dim] << "\t" << currcoords[ipt*dim+1] << "\t" << currcoords[ipt*dim+2] << std::endl;
          exit(0);
        }
        assert(fabs(naturalcoords[ipt*3]+naturalcoords[ipt*3+1]+naturalcoords[ipt*3+2]-1)<tol);
        for(int i=0;i<dim;++i){
          assert(fabs(naturalcoords[ipt*3]*cornercoords[i]+naturalcoords[ipt*3+1]*cornercoords[dim+i]+naturalcoords[ipt*3+2]*cornercoords[2*dim+i]-currcoords[ipt*dim+i])<tol);
        }
      }
    }
}//namespace
