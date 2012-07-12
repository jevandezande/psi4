/*! \file
    \ingroup CCDENSITY
    \brief Enter brief description of file here 
*/
#include <stdio.h>
#include <math.h>
#include "psi4-dec.h"
#include <libmints/mints.h>
#include <libciomr/libciomr.h>
#include <libpsio/psio.h>
#include <libiwl/iwl.h>
#include <libchkpt/chkpt.h>
#include <libdpd/dpd.h>
#include <libqt/qt.h>
#include <psifiles.h>
#include "MOInfo.h"
#include "Params.h"
#include "Frozen.h"
#define EXTERN
#include "globals.h"

namespace psi { namespace ccdensity {

/*
** densgrid_RHF(): Compute the values of the ground-state one-particle 
** density at a set of grid points.
**
** TDC, 7/2012
*/
void compute_delta(double **delta, double x, double y, double z);
int nmo, nso, nao; // global
double **scf, **u; // global
boost::shared_ptr<BasisSet> basis;
boost::shared_ptr<Wavefunction> wfn;

void densgrid_RHF(Options& options)
{
  double dens;
  double **D, **delta;
  double xmin, xmax, ymin, ymax, zmin, zmax;
  double xstep, ystep, zstep;
  int *order;
  double **scf_pitzer;

  wfn = Process::environment.reference_wavefunction();
  basis = wfn->basisset();

  nao = basis->nao();
  nso = moinfo.nso;
  nmo = moinfo.nmo;
  chkpt_init(PSIO_OPEN_OLD);
  scf_pitzer = chkpt_rd_scf();
  chkpt_close();

  D = moinfo.opdm; // A block matrix
  delta = block_matrix(nmo, nmo); // Dirac delta function 

  // Set up AO->SO transformation matrix (u)
  MintsHelper helper(options, 0);
  SharedMatrix aotoso = helper.petite_list(true)->aotoso();
  int *col_offset = new int[wfn->nirrep()];
  col_offset[0] = 0;
  for(int h=1; h < wfn->nirrep(); h++)
    col_offset[h] = col_offset[h-1] + aotoso->coldim(h-1);

  u = block_matrix(nao, nso);
  for(int h=0; h < wfn->nirrep(); h++)
    for(int j=0; j < aotoso->coldim(h); j++)
      for(int i=0; i < nao; i++)
        u[i][j+col_offset[h]] = aotoso->get(h, i, j);
  delete[] col_offset;

  /*** Arrange the SCF eigenvectors into QT ordering ***/
  order = moinfo.pitzer2qt;
  scf = block_matrix(nso, nmo);
  for(int i=0; i < nmo; i++) {
      int I = order[i];  /* Pitzer --> QT */
      for(int j=0; j < nso; j++) scf[j][I] = scf_pitzer[j][i];
    }

  xmin = -3.0; xmax = +3.0; xstep = 0.1;
  ymin = -3.0; ymax = +3.0; ystep = 0.1;
  zmin = -3.0; zmax = +3.0; zstep = 0.1;

  // Loop over points
  for(double x=xmin; x <= xmax; x += xstep) {
    for(double y=ymin; y <= ymax; y += ystep) {
      for(double z=zmin; z <= zmax; z += zstep) {

        // Compute delta function in Gaussian basis
        compute_delta(delta, x, y, z);

        dens = 0.0;
        for(int i=0; i < nmo; i++)
          for(int j=0; j < nmo; j++)
            dens += delta[i][j] * D[i][j];

      } // z
    }  // y
  } // x

  free_block(delta);
  free_block(scf);
}

void compute_delta(double **delta, double x, double y, double z)
{
  int i, j;
  double *phi_ao, *phi_so, *phi_mo;

  phi_ao = init_array(nao);  /* AO function values */
  phi_so = init_array(nso);  /* SO function values */
  phi_mo = init_array(nmo);  /* MO function values */

  basis->compute_phi(phi_ao, x, y, z);

  /*  for(i=0; i < nao; i++) printf("%d %20.10f\n", i, phi_ao[i]); */

  /* Transform the basis function values to the MO basis */
  C_DGEMV('t', nao, nso, 1.0, u[0], nso, phi_ao, 1, 0.0, phi_so, 1);

  C_DGEMV('t', nmo, nso, 1.0, scf[0], nmo, phi_so, 1, 0.0, phi_mo, 1);

  /* for(i=0; i < nmo; i++) printf("%d %20.10f\n", i, phi_mo[i]); */


  /* Build the MO-basis delta function */
  for(i=0; i < nmo; i++)
    for(j=0; j < nmo; j++)
      delta[i][j] = phi_mo[i] * phi_mo[j];

  free(phi_ao);
  free(phi_so);
  free(phi_mo);
}

}} // namespace psi::ccdensity
