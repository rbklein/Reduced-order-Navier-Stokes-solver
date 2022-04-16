#include <iostream>
#include <armadillo>
#include <cmath>
#include <complex>

#include "solver.h"

constexpr double PI = 3.14159265358979323846;

const arma::SpMat<double>& solver::D() const {
	return m_D;
}

const arma::SpMat<double>& solver::M() const {
	return m_M;
}

const arma::SpMat<double>& solver::G() const {
	return m_G;
}

const arma::SpMat<double>& solver::Om() const {
	return m_Omega;
}

const arma::SpMat<double>& solver::OmInv() const {
	return m_OmegaInv;
}

const arma::SpMat<double>& solver::L() const {
	return m_L;
}

double solver::nu() const {
	return m_nu;
}

const mesh& solver::getMesh() const {
	return m_mesh;
}

void solver::setupPressurePoissonMatrix() {

	switch (m_pSolver) {
	case(POISSON_SOLVER::DIRECT):

		//calculate poisson matrix
		m_L = m_M * m_OmegaInv * m_G;

		//set row to ones to make non-singular (sets non-weighted average pressure)
		for (arma::uword i = 0; i < (m_mesh.getNumCellsX() * m_mesh.getNumCellsY()); ++i) {
			m_L(0, i) = 1.0;
		}

		break;
	case(POISSON_SOLVER::FOURIER):

		//check if periodic

		const arma::field<cell>& CellsP = m_mesh.getCellsP();

		m_Lhat = arma::Mat<std::complex<double>>(m_mesh.getNumCellsY(), m_mesh.getNumCellsX());

		using namespace std::complex_literals;

		for (arma::uword i = 0; i < m_mesh.getNumCellsY(); ++i) {
			for (arma::uword j = 0; j < m_mesh.getNumCellsX(); ++j) {

				//column major ordering: this will be slow (but executed once)
				m_Lhat(i, j) = (CellsP(i, j).dx * CellsP(i, j).dy) * -4.0 * ((1.0 / (CellsP(i, j).dx * CellsP(i, j).dx)) * sin(i * PI / m_mesh.getNumCellsX()) * sin(i * PI / m_mesh.getNumCellsX()) +
					(1.0 / (CellsP(i, j).dy * CellsP(i, j).dy)) * sin(j * PI / m_mesh.getNumCellsY()) * sin(j * PI / m_mesh.getNumCellsY())) + 0.0i; 

				//m_Lhat(i, j) =  - 4.0 * (sin(j * PI / m_mesh.getNumCellsX()) * sin(j * PI / m_mesh.getNumCellsX()) + sin(i * PI / m_mesh.getNumCellsY()) * sin(i * PI / m_mesh.getNumCellsY())) + 0.0i;

			}
		}

		m_Lhat(0, 0) = 1.0 + 0.0i;

		break;
	}
	
}

arma::Col<double> solver::poissonSolve(const arma::Col<double>& MV) const {

	switch (m_pSolver) {
	case(POISSON_SOLVER::DIRECT):

		return arma::spsolve(m_L, MV);

		break;
	case(POISSON_SOLVER::FOURIER):

		using namespace std::complex_literals;

		arma::Mat<double> f = arma::reshape(MV, m_mesh.getNumCellsY(), m_mesh.getNumCellsX());

		arma::Mat<std::complex<double>> fhat = arma::fft2(f);

		fhat(0, 0) = 0.0 + 0.0i;

		return arma::real(arma::ifft2(fhat / m_Lhat)).as_col();

		break;
	}

}

//only support periodic boundary conditions atm
arma::Col<double> solver::interpolateVelocity(const arma::Col<double>& vel) {

	arma::uword num = m_mesh.getNumCellsX() * m_mesh.getNumCellsY();

	arma::Col<double> velInterp = arma::zeros(2 * num);

	const arma::field<cell>& CellsU = m_mesh.getCellsU();
	const arma::field<cell>& CellsV = m_mesh.getCellsV();
	const arma::field<cell>& CellsP = m_mesh.getCellsP();

	for (arma::uword i = 0; i < m_mesh.getNumCellsY(); ++i) {
		for (arma::uword j = 0; j < m_mesh.getNumCellsX(); ++j) {

			if (i != (m_mesh.getNumCellsY() - 1))
				velInterp(CellsP(i, j).vectorIndex + num) = 0.5 * (vel(CellsV(i, j).vectorIndex) + vel(CellsV(i + 1, j).vectorIndex));
			else
				velInterp(CellsP(i, j).vectorIndex + num) = 0.5 * (vel(CellsV(i, j).vectorIndex) + vel(CellsV(0, j).vectorIndex));

			if (j != (m_mesh.getNumCellsX() - 1))
				velInterp(CellsP(i, j).vectorIndex) = 0.5 * (vel(CellsU(i, j).vectorIndex) + vel(CellsU(i, j + 1).vectorIndex));
			else
				velInterp(CellsP(i, j).vectorIndex) = 0.5 * (vel(CellsU(i, j).vectorIndex) + vel(CellsU(i, 0).vectorIndex));

		}
	}

	return velInterp;
}