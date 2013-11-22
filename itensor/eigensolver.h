//
// Distributed under the ITensor Library License, Version 1.0.
//    (See accompanying LICENSE file.)
//
#ifndef __ITENSOR_EIGENSOLVER_H
#define __ITENSOR_EIGENSOLVER_H
#include "iqcombiner.h"

#define Cout std::cout
#define Endl std::endl
#define Format boost::format

//
// Use basic power method to find first n==vecs.size()
// eigenvalues and eigenvectors of the matrix A.
// (A must provide the .product(Tensor in,Tensor& out) method)
// Returns the eigenvalues and vecs are equal to the eigenvectors on return.
//
template <typename BigMatrixT, typename Tensor>
std::vector<Real>
powerMethod(const BigMatrixT& A, 
            std::vector<Tensor>& vecs,
            const OptSet& opts = Global::opts());

//
// Uses the Davidson algorithm to find the 
// minimal eigenvector of the sparse matrix A.
// (BigMatrixT objects must implement the methods product, size and diag.)
// Returns the minimal eigenvalue lambda such that
// A phi = lambda phi.
//
template <class BigMatrixT, class Tensor> 
Real 
davidson(const BigMatrixT& A, Tensor& phi,
         const OptSet& opts = Global::opts());

//
// Uses Davidson to find the N smallest eigenvectors
// of the sparse matrix A, given a vector of N initial
// guesses (zero indexed).
// (BigMatrixT objects must implement the methods product, size and diag.)
// Returns a vector of the N smallest eigenvalues corresponding
// to the set of eigenvectors phi.
//
template <class BigMatrixT, class Tensor> 
std::vector<Real>
davidson(const BigMatrixT& A, 
         std::vector<Tensor>& phi,
         const OptSet& opts = Global::opts());

template <class BigMatrixT, class Tensor> 
std::vector<Complex>
complexDavidson(const BigMatrixT& A, 
                std::vector<Tensor>& phi,
                const OptSet& opts = Global::opts());

//
// Uses the Davidson algorithm to find the minimal
// eigenvector of the generalized eigenvalue problem
// A phi = lambda B phi.
// (B should have positive definite eigenvalues.)
//
template <class BigMatrixTA, class BigMatrixTB, class Tensor> 
Real
nonOrthDavidson(const BigMatrixTA& A, 
                const BigMatrixTB& B, 
                Tensor& phi, 
                const OptSet& opts = Global::opts());



//
//
// Implementations
//
//



template <typename BigMatrixT, typename Tensor>
std::vector<Real>
powerMethod(const BigMatrixT& A, 
            std::vector<Tensor>& vecs,
            const OptSet& opts)
    {
    const size_t nget = vecs.size();
    const int maxiter = 1000;
    const Real errgoal_ = opts.getReal("ErrGoal",1E-4);
    const int dlevel = opts.getInt("DebugLevel",0);
    std::vector<Real> eigs(nget,1000);
    for(size_t t = 0; t < nget; ++t)
        {
        Tensor& v = vecs.at(t);
        Tensor vp;
        Real& lambda = eigs.at(t);
        Real last_lambda = 1000;

        v /= v.norm();
        for(int ii = 1; ii <= maxiter; ++ii)
            {
            A.product(v,vp);
            v = vp;
            for(size_t j = 0; j < t; ++j)
                {
                v += (-eigs.at(j)*vecs.at(j)*BraKet(vecs.at(j),v));
                }
            last_lambda = lambda;
            lambda = v.norm();
            v /= lambda;
            if(dlevel >= 1)
                Cout << Format("%d %d %.10f") % t % ii % lambda << Endl;
            if(fabs(lambda-last_lambda) < errgoal_)
                {
                break;
                }
            }
        }
    return eigs;
    }


//Function object which applies the mapping
// f(x,theta) = 1/(theta - x)
class DavidsonPrecond
    {
    public:
        DavidsonPrecond(Real theta)
            : theta_(theta)
            { }
        Real
        operator()(Real val) const
            {
            if(theta_ == val)
                return 0;
            else
                return 1.0/(theta_-val);
            }
    private:
        Real theta_;
    };

//Function object which applies the mapping
// f(x,theta) = 1/(theta - 1)
class LanczosPrecond
    {
    public:
        LanczosPrecond(Real theta)
            : theta_(theta)
            { }
        Real
        operator()(Real val) const
            {
            return 1.0/(theta_-1+1E-33);
            }
    private:
        Real theta_;
    };

//Function object which applies the mapping
// f(x) = (x < cut ? 0 : 1/x);
class PseudoInverter
    {
    public:
        PseudoInverter(Real cut = MIN_CUT)
            :
            cut_(cut)
            { }

        Real
        operator()(Real val) const
            {
            if(fabs(val) < cut_)
                return 0;
            else
                return 1./val;
            }
    private:
        Real cut_;
    };


template <class BigMatrixT, class Tensor> 
Real
davidson(const BigMatrixT& A, Tensor& phi,
         const OptSet& opts)
    {
    std::vector<Tensor> v(1);
    v.front() = phi;
    std::vector<Real> eigs = davidson(A,v,opts);
    phi = v.front();
    return eigs.front();
    }

template <class BigMatrixT, class Tensor> 
std::vector<Real>
davidson(const BigMatrixT& A, 
         std::vector<Tensor>& phi,
         const OptSet& opts)
    {
    const int debug_level_ = opts.getInt("DebugLevel",-1);
    const Real Approx0 = 1E-12;
    std::vector<Complex> ceigs = complexDavidson(A,phi,opts);
    std::vector<Real> eigs(ceigs.size());
    for(size_t j = 0; j < ceigs.size(); ++j)
        {
        eigs.at(j) = ceigs.at(j).real();
        if(debug_level_ > 2 && ceigs.at(j).imag() > Approx0)
            {
            Cout << Format("Warning: dropping imaginary part of eigs[%d] = (%.4E,%.4E).")
                    % j
                    % ceigs.at(j).real()
                    % ceigs.at(j).imag()
                 << Endl;
            }
        }
    return eigs;
    }

int inline
findEig(int num,          //zero-indexed; so is return value
        const Vector& DR, //real part of eigenvalues
        const Vector& DI) //imag part of eigenvalues
    {
    const int L = DR.Length();
#ifdef DEBUG
    if(DI.Length() != L) Error("Vectors must have same length in findEig");
#endif
    Vector A2(L);
    Real maxj = -1;
    int w = -1;
    for(int ii = 1; ii <= L; ++ii) 
        {
        A2(ii) = sqr(DR(ii))+sqr(DI(ii));
        if(A2(ii) > maxj) 
            {
            maxj = A2(ii);
            w = ii;
            }
        }
    for(int j = 1; j <= num; ++j)
        {
        Real nmax = -1;
        for(int ii = 1; ii <= L; ++ii)
            {
            if(A2(ii) > nmax && A2(ii) < maxj)
                {
                nmax = A2(ii);
                w = ii;
                }
            }
        maxj = nmax;
        }
    //Cout << "DR = " << DR;
    //Cout << Format("Eig %d (%f) at position %d") % num % DR(w) % w << Endl;
    return (w-1);
    }


template <class BigMatrixT, class Tensor> 
std::vector<Complex>
complexDavidson(const BigMatrixT& A, 
                std::vector<Tensor>& phi,
                const OptSet& opts)
    {
    const int maxiter_ = opts.getInt("MaxIter",2);
    const Real errgoal_ = opts.getReal("ErrGoal",1E-4);
    const int debug_level_ = opts.getInt("DebugLevel",-1);
    const int miniter_ = opts.getInt("MinIter",1);
    const bool hermitian = opts.getBool("Hermitian",true);

    const Real Approx0 = 1E-12;

    const size_t nget = phi.size();
    if(nget == 0)
        {
        Error("No initial vectors passed to davidson.");
        }

    for(size_t j = 0; j < nget; ++j)
        {
        const Real nrm = phi[j].norm();
        if(nrm == 0.0)
            Error("norm of 0 in davidson");
        phi[j] *= 1.0/nrm;
        }

    bool complex_diag = false;

    const int maxsize = A.size();
    const int actual_maxiter = min(maxiter_,maxsize-1);
    if(debug_level_ >= 2)
        {
        Cout << Format("maxsize-1 = %d, maxiter = %d, actual_maxiter = %d") 
                % (maxsize-1) % maxiter_ % actual_maxiter << Endl;
        }

    if(phi.front().indices().dim() != maxsize)
        {
        Print(phi.front().indices().dim());
        Print(A.size());
        Error("davidson: size of initial vector should match linear matrix size");
        }

    std::vector<Tensor> V(actual_maxiter+2),
                       AV(actual_maxiter+2);

    //Storage for Matrix that gets diagonalized 
    Matrix MR(actual_maxiter+2,actual_maxiter+2),
           MI(actual_maxiter+2,actual_maxiter+2);
    MR = NAN; //set to NAN to ensure failure if we use uninitialized elements
    MI = NAN;

    //Mref holds current projection of A into V's
    MatrixRef MrefR(MR.SubMatrix(1,1,1,1)),
              MrefI(MI.SubMatrix(1,1,1,1));

    //Get diagonal of A to use later
    const Tensor Adiag = A.diag();

    Complex last_lambda(1000,0);

    Real qnorm = NAN;

    V[0] = phi.front();
    A.product(V[0],AV[0]);

    Complex z = BraKet(V[0],AV[0]);
    const Real initEn = z.real();

    if(debug_level_ > 2)
        Cout << Format("Initial Davidson energy = %.10f") % initEn << Endl;

    size_t t = 0; //which eigenvector we are currently targeting
    Vector D,DI;
    Matrix UR,UI;

    std::vector<Complex> eigs(nget,Complex(NAN,NAN));

    int iter = 0;
    for(int ii = 0; ii <= actual_maxiter; ++ii)
        {
        //Diagonalize conj(V)*A*V
        //and compute the residual q

        const int ni = ii+1; 
        Tensor& q = V.at(ni);
        Tensor& phi_t = phi.at(t);
        Complex& lambda = eigs.at(t);

        //Step A (or I) of Davidson (1975)
        if(ii == 0)
            {
            lambda = Complex(initEn,0.);
            MrefR = lambda.real();
            MrefI = 0;
            //Calculate residual q
            q = V[0];
            q *= -lambda.real();
            q += AV[0]; 
            }
        else // ii != 0
            {
            //Diagonalize M
            int w = t; //w 'which' variable needed because
                       //eigs returned by ComplexEigenValues and
                       //GenEigenvalues aren't sorted
            if(complex_diag)
                {
                if(hermitian)
                    {
                    HermitianEigenvalues(MrefR,MrefI,D,UR,UI);
                    DI.ReDimension(D.Length());
                    DI = 0;
                    }
                else
                    {
                    ComplexEigenvalues(MrefR,MrefI,D,DI,UR,UI);
                    w = findEig(t,D,DI);
                    }

                //Compute corresponding eigenvector
                //phi_t of A from the min evec of M
                //(and start calculating residual q)

                phi_t = (UR(1,1+w)*Complex_1+UI(1,1+w)*Complex_i)*V[0];
                q   = (UR(1,1+w)*Complex_1+UI(1,1+w)*Complex_i)*AV[0];
                for(int k = 1; k <= ii; ++k)
                    {
                    const Complex cfac = (UR(k+1,1+w)*Complex_1+UI(k+1,1+w)*Complex_i);
                    phi_t += cfac*V[k];
                    q   += cfac*AV[k];
                    }
                }
            else
                {
                bool complex_evec = false;
                if(hermitian)
                    {
                    EigenValues(MrefR,D,UR);
                    DI.ReDimension(D.Length());
                    DI = 0;
                    }
                else
                    {
                    GenEigenValues(MrefR,D,DI,UR,UI);
                    w = findEig(t,D,DI);
                    if(Norm(UI.Column(1+w)) > Approx0)
                        complex_evec = true;
                    }

                //Compute corresponding eigenvector
                //phi_t of A from the min evec of M
                //(and start calculating residual q)
                phi_t = UR(1,1+w)*V[0];
                q   = UR(1,1+w)*AV[0];
                for(int k = 1; k <= ii; ++k)
                    {
                    phi_t += UR(k+1,1+w)*V[k];
                    q   += UR(k+1,1+w)*AV[k];
                    }
                if(complex_evec)
                    {
                    phi_t += Complex_i*UI(1,1+w)*V[0];
                    q   += Complex_i*UI(1,1+w)*AV[0];
                    for(int k = 1; k <= ii; ++k)
                        {
                        phi_t += Complex_i*UI(k+1,1+w)*V[k];
                        q   += Complex_i*UI(k+1,1+w)*AV[k];
                        }
                    }
                }

            //lambda is the w^th eigenvalue of M
            lambda = Complex(D(1+w),DI(1+w));

            //Step B of Davidson (1975)
            //Calculate residual q
            if(lambda.imag() <= Approx0)
                q += (-lambda.real())*phi_t;
            else
                q += (-lambda)*phi_t;

            //Fix sign
            if(UR(1,1+w) < 0)
                {
                phi_t *= -1;
                q *= -1;
                }

            if(debug_level_ >= 3)
                {
                Cout << "complex_diag = " 
                     << (complex_diag ? "true" : "false") << Endl;
                Cout << "D = " << D;
                Cout << Format("lambda = %.10f") % D(1) << Endl;
                }

            }

        //Step C of Davidson (1975)
        //Check convergence
        qnorm = q.norm();

        const bool converged = (qnorm < errgoal_ && abs(lambda-last_lambda) < errgoal_) 
                               || qnorm < max(Approx0,errgoal_ * 1E-3);

        last_lambda = lambda;

        if((qnorm < 1E-20) || (converged && ii >= miniter_) || (ii == actual_maxiter))
            {
            if(t < (nget-1) && ii < actual_maxiter) 
                {
                ++t;
                last_lambda = Complex(1000,0);
                }
            else
                {
                if(debug_level_ >= 3) //Explain why breaking out of Davidson loop early
                    {
                    if((qnorm < errgoal_ && abs(lambda-last_lambda) < errgoal_))
                        {
                        Cout << Format("Exiting Davidson because errgoal=%.0E reached") % errgoal_ << Endl;
                        }
                    else
                    if(ii < miniter_ || qnorm < max(Approx0,errgoal_ * 1.0e-3))
                        {
                        Cout << Format("Exiting Davidson because small residual=%.0E obtained") % qnorm << Endl;
                        }
                    else
                    if(ii == actual_maxiter)
                        {
                        Cout << "Exiting Davidson because ii == actual_maxiter" << Endl;
                        }
                    }

                goto done;
                }
            }
        
        if(debug_level_ >= 2 || (ii == 0 && debug_level_ >= 1))
            {
            Cout << Format("I %d q %.0E E") % iter % qnorm;
            for(size_t j = 0; j < eigs.size(); ++j)
                {
                if(std::isnan(eigs[j].real())) break;
                if(fabs(eigs[j].imag()) > Approx0)
                    Cout << Format(" (%.10f,%.10f)") % eigs[j].real() % eigs[j].imag();
                else
                    Cout << Format(" %.10f") % eigs[j].real();
                }
            Cout << Endl;
            }

        //Compute next trial vector by
        //first applying Davidson preconditioner
        //formula then orthogonalizing against
        //other vectors

        //Step D of Davidson (1975)
        //Apply Davidson preconditioner
        if(!Adiag.isNull())
            {
            DavidsonPrecond dp(lambda.real());
            Tensor cond(Adiag);
            cond.mapElems(dp);
            q /= cond;
            }

        //Step E and F of Davidson (1975)
        //Do Gram-Schmidt on d (Npass times)
        //to include it in the subbasis
        const int Npass = 1;
        std::vector<Complex> Vq(ni);

        int count = 0;
        for(int pass = 1; pass <= Npass; ++pass)
            {
            ++count;
            for(int k = 0; k < ni; ++k)
                {
                Vq[k] = BraKet(V[k],q);
                }

            for(int k = 0; k < ni; ++k)
                {
                q += (-Vq[k].real())*V[k];
                if(Vq[k].imag() != 0)
                    {
                    q += (-Vq[k].imag()*Complex_i)*V[k];
                    }
                }

            Real qn = q.norm();

            if(qn < 1E-10)
                {
                //Orthogonalization failure,
                //try randomizing
                if(debug_level_ >= 2)
                    Cout << "Vector not independent, randomizing" << Endl;
                q = V.at(ni-1);
                q.randomize();

                if(ni >= maxsize)
                    {
                    //Not be possible to orthogonalize if
                    //max size of q (vecSize after randomize)
                    //is size of current basis
                    if(debug_level_ >= 3)
                        Cout << "Breaking out of Davidson: max Hilbert space size reached" << Endl;
                    goto done;
                    }

                if(count > Npass * 3)
                    {
                    // Maybe the size of the matrix is only 1?
                    if(debug_level_ >= 3)
                        Cout << "Breaking out of Davidson: count too big" << Endl;
                    goto done;
                    }

                qn = q.norm();
                --pass;
                }

            q *= 1./qn;
            }

        if(debug_level_ >= 3)
            {
            if(fabs(q.norm()-1.0) > 1E-10)
                {
                Print(q.norm());
                Error("q not normalized after Gram Schmidt.");
                }
            }


        //Step G of Davidson (1975)
        //Expand AV and M
        //for next step
        A.product(V[ni],AV[ni]);

        //Step H of Davidson (1975)
        //Add new row and column to M
        MrefR << MR.SubMatrix(1,ni+1,1,ni+1);
        MrefI << MI.SubMatrix(1,ni+1,1,ni+1);
        Vector newColR(ni+1),
               newColI(ni+1);
        for(int k = 0; k <= ni; ++k)
            {
            z = BraKet(V.at(k),AV.at(ni));
            newColR(k+1) = z.real();
            newColI(k+1) = z.imag();
            }
        MrefR.Column(ni+1) = newColR;
        MrefI.Column(ni+1) = newColI;

        if(hermitian)
            {
            MrefR.Row(ni+1) = newColR;
            MrefI.Row(ni+1) = -newColI;
            }
        else
            {
            Vector newRowR(ni+1),
                   newRowI(ni+1);
            for(int k = 0; k < ni; ++k)
                {
                z = BraKet(V.at(ni),AV.at(k));
                newRowR(k+1) = z.real();
                newRowI(k+1) = z.imag();
                }
            newRowR(ni+1) = newColR(ni+1);
            newRowI(ni+1) = newColI(ni+1);
            MrefR.Row(ni+1) = newRowR;
            MrefI.Row(ni+1) = newRowI;
            }

        if(!complex_diag && Norm(newColI) > errgoal_)
            {
            complex_diag = true;
            }

        ++iter;

        } //for(ii)

    done:

    //Compute any remaining eigenvalues and eigenvectors requested
    //(zero indexed) value of t indicates how many have been "targeted" so far
    for(size_t j = t+1; j < nget; ++j)
        {
        eigs.at(j) = Complex(D(1+j),DI(1+j));

        Tensor& phi_j = phi.at(j);
        const bool complex_evec = (Norm(UI.Column(1+t)) > Approx0);

        const int Nr = UR.Nrows();

        phi_j = UR(1,1+t)*V[0];
        for(int k = 1; k < Nr; ++k)
            {
            phi_j += UR(1+k,1+t)*V[k];
            }
        if(complex_evec)
            {
            phi_j += Complex_i*UI(1,1+t)*V[0];
            for(int k = 1; k < Nr; ++k)
                {
                phi_j += Complex_i*UI(1+k,1+t)*V[k];
                }
            }
        }

    if(debug_level_ >= 3)
        {
        //Check V's are orthonormal
        Matrix Vo_final(iter+1,iter+1); 
        Vo_final = NAN;
        for(int r = 1; r <= iter+1; ++r)
        for(int c = r; c <= iter+1; ++c)
            {
            z = BraKet(V[r-1],V[c-1]);
            Vo_final(r,c) = abs(z);
            Vo_final(c,r) = Vo_final(r,c);
            }
        Cout << "Vo_final = " << Endl;
        Cout << Vo_final;
        }

    if(debug_level_ > 0)
        {
        Cout << Format("I %d q %.0E E") % iter % qnorm;
        for(size_t j = 0; j < eigs.size(); ++j)
            {
            if(std::isnan(eigs[j].real())) break;
            if(fabs(eigs[j].imag()) > Approx0)
                Cout << Format(" (%.10f,%.10f)") % eigs[j].real() % eigs[j].imag();
            else
                Cout << Format(" %.10f") % eigs[j].real();
            }
        Cout << Endl;
        }

    return eigs;

    } //complexDavidson

template <class BigMatrixTA, class BigMatrixTB, class Tensor> 
Real
nonOrthDavidson(const BigMatrixTA& A, 
                const BigMatrixTB& B, 
                Tensor& phi, 
                const OptSet& opts)
    {
    int maxiter_ = opts.getInt("MaxIter",2);
    Real errgoal_ = opts.getReal("ErrGoal",1E-4);
    //int numget_ = opts.getInt("NumGet",1);
    int debug_level_ = opts.getInt("DebugLevel",-1);
    //int miniter_ = opts.getInt("MinIter",1);

    //B-normalize phi
    {
    Tensor Bphi;
    B.product(phi,Bphi);
    Real phiBphi = Dot(conj(phi),Bphi);
    phi *= 1.0/sqrt(phiBphi);
    }


    const int maxsize = A.size();
    const int actual_maxiter = min(maxiter_,maxsize);
    Real lambda = 1E30, 
         last_lambda = lambda,
         qnorm = 1E30;

    std::vector<Tensor> V(actual_maxiter+2),
                       AV(actual_maxiter+2),
                       BV(actual_maxiter+2);

    //Storage for Matrix that gets diagonalized 
    //M is the projected form of A
    //N is the projected form of B
    Matrix M(actual_maxiter+2,actual_maxiter+2),
           N(actual_maxiter+2,actual_maxiter+2);

    MatrixRef Mref(M.SubMatrix(1,1,1,1)),
              Nref(N.SubMatrix(1,1,1,1));

    Vector D;
    Matrix U;

    //Get diagonal of A,B to use later
    //Tensor Adiag(phi);
    //A.diag(Adiag);
    //Tensor Bdiag(phi);
    //B.diag(Bdiag);

    int iter = 0;
    for(int ii = 1; ii <= actual_maxiter; ++ii)
        {
        ++iter;
        //Diagonalize conj(V)*A*V
        //and compute the residual q
        Tensor q;
        if(ii == 1)
            {
            V[1] = phi;
            A.product(V[1],AV[1]);
            B.product(V[1],BV[1]);

            //No need to diagonalize
            Mref = Dot(conj(V[1]),AV[1]);
            Nref = Dot(conj(V[1]),BV[1]);
            lambda = Mref(1,1)/(Nref(1,1)+1E-33);

            //Calculate residual q
            q = BV[1];
            q *= -lambda;
            q += AV[1]; 
            }
        else // ii != 1
            {
            //Diagonalize M
            //Print(Mref);
            //std::cerr << "\n";
            //Print(Nref);
            //std::cerr << "\n";

            GeneralizedEV(Mref,Nref,D,U);

            //lambda is the minimum eigenvalue of M
            lambda = D(1);

            //Calculate residual q
            q   = U(1,1)*AV[1];
            q   = U(1,1)*(AV[1]-lambda*BV[1]);
            for(int k = 2; k <= ii; ++k)
                {
                q = U(k,1)*(AV[k]-lambda*BV[k]);
                }
            }

        //Check convergence
        qnorm = q.norm();
        if( (qnorm < errgoal_ && fabs(lambda-last_lambda) < errgoal_) 
            || qnorm < 1E-12 )
            {
            break; //Out of ii loop to return
            }

        /*
        if(debug_level_ >= 1 && qnorm > 3)
            {
            std::cerr << "Large qnorm = " << qnorm << "\n";
            Print(Mref);
            Vector D;
            Matrix U;
            EigenValues(Mref,D,U);
            Print(D);

            std::cerr << "ii = " << ii  << "\n";
            Matrix Vorth(ii,ii);
            for(int i = 1; i <= ii; ++i)
            for(int j = i; j <= ii; ++j)
                {
                Vorth(i,j) = Dot(conj(V[i]),V[j]);
                Vorth(j,i) = Vorth(i,j);
                }
            Print(Vorth);
            exit(0);
            }
        */

        if(debug_level_ > 1 || (ii == 1 && debug_level_ > 0))
            {
            Cout << Format("I %d q %.0E E %.10f")
                           % ii
                           % qnorm
                           % lambda 
                           << Endl;
            }

        //Apply generalized Davidson preconditioner
        //{
        //Tensor cond = lambda*Bdiag - Adiag;
        //PseudoInverter inv;
        //cond.mapElems(inv);
        //q /= cond;
        //}

        /*
         * According to Kalamboukis Gram-Schmidt not needed,
         * presumably because the new entries of N will
         * contain the B-overlap of any new vectors and thus
         * account for their non-orthogonality.
         * Since B-orthogonalizing new vectors would be quite 
         * expensive, what about doing regular Gram-Schmidt?
         * The idea is it won't hurt if it's a little wrong
         * since N will account for it, but if B is close
         * to the identity then it should help a lot.
         *
         */

//#define DO_GRAM_SCHMIDT

        //Do Gram-Schmidt on xi
        //to include it in the subbasis
        Tensor& d = V[ii+1];
        d = q;
#ifdef  DO_GRAM_SCHMIDT
        Vector Vd(ii);
        for(int k = 1; k <= ii; ++k)
            {
            Vd(k) = Dot(conj(V[k]),d);
            }
        d = Vd(1)*V[1];
        for(int k = 2; k <= ii; ++k)
            {
            d += Vd(k)*V[k];
            }
        d *= -1;
        d += q;
#endif//DO_GRAM_SCHMIDT
        d *= 1.0/(d.norm()+1E-33);

        last_lambda = lambda;

        //Expand AV, M and BV, N
        //for next step
        if(ii < actual_maxiter)
            {
            A.product(d,AV[ii+1]);
            B.product(d,BV[ii+1]);


            //Add new row and column to N
            Vector newCol(ii+1);
            Nref << N.SubMatrix(1,ii+1,1,ii+1);
            for(int k = 1; k <= ii+1; ++k)
                {
                newCol(k) = Dot(conj(V[k]),BV[ii+1]);

                if(newCol(k) < 0)
                    {
                    //if(k > 1)
                        //Error("Can't fix sign of new basis vector");
                    newCol(k) *= -1;
                    BV[ii+1] *= -1;
                    d *= -1;
                    }
                }
            Nref.Column(ii+1) = newCol;
            Nref.Row(ii+1) = newCol;

            //Add new row and column to M
            Mref << M.SubMatrix(1,ii+1,1,ii+1);
            for(int k = 1; k <= ii+1; ++k)
                {
                newCol(k) = Dot(conj(V[k]),AV[ii+1]);
                }
            Mref.Column(ii+1) = newCol;
            Mref.Row(ii+1) = newCol;

            }

        } //for(ii)

    if(debug_level_ > 0)
        {
        Cout << Format("I %d q %.0E E %.10f")
                       % iter
                       % qnorm
                       % lambda 
                       << Endl;
        }

    //Compute eigenvector phi before returning
#ifdef DEBUG
    if(U.Nrows() != iter)
        {
        Print(U.Nrows());
        Print(iter);
        Error("Wrong size: U.Nrows() != iter");
        }
#endif
    phi = U(1,1)*V[1];
    for(int k = 2; k <= iter; ++k)
        {
        phi += U(k,1)*V[k];
        }

    return lambda;

    } //nonOrthDavidson

/*
template<class Tensor>
void
orthog(std::vector<Tensor>& T, int num, int numpass, int start)
    {
    const int size = T[start].maxSize();
    if(num > size)
        {
        Print(num);
        Print(size);
        Error("num > size");
        }

    for(int n = start; n <= num+(start-1); ++n)
        { 
        Tensor& col = T.at(n);
        Real norm = col.norm();
        if(norm == 0)
            {
            col.randomize();
            norm = col.norm();
            //If norm still zero, may be
            //an IQTensor with no blocks
            if(norm == 0)
                {
                PrintDat(col);
                Error("Couldn't randomize column");
                }
            }
        col /= norm;
        col.scaleTo(1);
        
        if(n == start) continue;

        for(int pass = 1; pass <= numpass; ++pass)
            {
            Vector dps(n-start);
            for(int m = start; m < n; ++m)
                {
                dps(m-start+1) = Dot(conj(col),T.at(m));
                }
            Tensor ovrlp = dps(1)*T.at(start);
            for(int m = start+1; m < n; ++m)
                {
                ovrlp += dps(m-start+1)*T.at(m);
                }
            ovrlp *= -1;
            col += ovrlp;

            norm = col.norm();
            if(norm == 0)
                {
                Error("Couldn't normalize column");
                }
            col /= norm;
            col.scaleTo(1);

            }
        }
    } // orthog(vector<Tensor> ... )
    */

#undef Cout
#undef Endl
#undef Format

#endif
