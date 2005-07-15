/* -*- linux-c -*- */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/times.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>
#include "cmc.h"
#include "cmc_vars.h"

/* a fast square function */
inline double sqr(double x)
{
        return(x*x);
}

/* a fast cube function */
inline double cub(double x)
{
        return(x*x*x);
}

/* toggle debugging */
void toggle_debugging(int signal)
{
	if (debug) {
		fprintf(stderr, "toggle_debugging(): turning debugging off on signal %d\n", signal);
		debug = 0;
	} else {
		fprintf(stderr, "toggle_debugging(): turning debugging on on signal %d\n", signal);
		debug = 1;
	}
}

/* close buffers, then exit */
void exit_cleanly(int signal)
{
	close_buffers();
	free_arrays();

	exit(signal);
}

void free_arrays(void){
	free(mass_pc); free(densities_r); free(no_star_r); 
	free(ave_mass_r); free(mass_r);
	free(star); free(binary);
}

/* GSL error handler */
void sf_gsl_errhandler(const char *reason, const char *file, int line, int gsl_errno)
{
	fprintf(stderr, "gsl: %s:%d: ERROR: %s\n", file, line, reason);
	exit_cleanly(gsl_errno);
}

void setup_sub_time_step(void){
	long p, i, k, si_minus_p, si_plus_p, sub_imin, si, zk;
	double Ai, Dt_local, m_avg, w2_avg, zr_min, zr_max;
	
	/********** setup sub time step *************************/
	/* Timestep obtained from GetTimeStep is based on the 
	 * relaxation time in the core. Now look for a suitable 
	 * boundary between core and halo. */

	if (sub.count == 0) {		/** do a full timestep **/
		p = AVEKERNEL;
		sub.N_MAX = clus.N_MAX;	/* default -- if Dt does not 
					   change much up to r_h */
		sub.FACTOR = 1;
		sub_imin = 2000;

		for (si = sub_imin; si <= clus.N_MAX; si += clus.N_MAX / 100) {
			si_minus_p = si - p;
			si_plus_p = si + p + 1;

			if (si_minus_p < 0) {
				si_minus_p = 0;
				si_plus_p = 2 * p + 1;
			} else if (si_plus_p > clus.N_MAX) {
				si_plus_p = clus.N_MAX;
				si_minus_p = clus.N_MAX - 2 * p - 1;
			}

			/* calculate Ai for zone */
			m_avg = 0.0;
			w2_avg = 0.0;
			zk = 2 * p + 2;
			for (i = si_minus_p; i <= si_plus_p; i++) {
				k = i;
				m_avg += star[k].m;
				w2_avg += star[k].m * (star[k].vr * star[k].vr + star[k].vt * star[k].vt);
			}
			m_avg = m_avg / zk;
			w2_avg = w2_avg * 2.0 / m_avg / zk;
			zr_min = star[si_minus_p].r;
			zr_max = star[si_plus_p].r;
			Ai = 6.0 * zk * sqr(m_avg) / (cub(zr_max) - cub(zr_min)) / sqrt(cub(w2_avg));

			/* FIXME: subzoning does not work correctly with the new relaxation
			   scheme! */
			Dt_local = sqr(sin(THETASEMAX)) / Ai * clus.N_STAR / DT_FACTOR;
			/* DEBUG */
			Dt_local *= Mtotal;
			/* DEBUG */

			/* to turn off sub-zoning */
			if (!SUBZONING) {
				break;
			}
			
//			if (Dt_local >= Dt * 500 && sub.FACTOR < 500) {
//				if (si * 1.0 / clus.N_MAX <
//				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 500.0) {
//					sub.FACTOR = 500;
//					sub.N_MAX = si;
//					break;
//				}
//			}
//			if (Dt_local >= Dt * 100 && sub.FACTOR < 100) {
//				if (si * 1.0 / clus.N_MAX <
//				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 100.0) {
//					sub.FACTOR = 100;
//					sub.N_MAX = si;
//				}
//			} else if (Dt_local >= Dt * 50 && sub.FACTOR < 50) {
//				if (si * 1.0 / clus.N_MAX <
//				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 50.0) {
//					sub.FACTOR = 50;
//					sub.N_MAX = si;
//				}
//			} else 
			if (Dt_local >= Dt * 25 && sub.FACTOR < 25) {
				if (si * 1.0 / clus.N_MAX <
				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 25.0) {
					sub.FACTOR = 25;
					sub.N_MAX = si;
				}
			} else 
			if (Dt_local >= Dt * 10 && sub.FACTOR < 10) {
				if (si * 1.0 / clus.N_MAX <
				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 10.0) {
					sub.FACTOR = 10;
					sub.N_MAX = si;
				}
			} else 
			if (Dt_local >= Dt * 5 && sub.FACTOR < 5) {
				if (si * 1.0 / clus.N_MAX <
				    1.0 / sub.FACTOR * (sub.N_MAX * 1.0 / clus.N_MAX * sub.FACTOR + 1.0) - 1.0 / 5.0) {
					sub.FACTOR = 5;
					sub.N_MAX = si;
				}
			} else 
			if (Dt_local >= Dt * 2 && sub.FACTOR < 2 && si < clus.N_MAX / 2) {
				sub.FACTOR = 2;
				sub.N_MAX = si;
			} else if (si > clus.N_MAX / 2) {
				break;
			}
		}

		sub.rmax = star[sub.N_MAX].r;
		/* I'm pretty sure this should be 0 to turn off sub-zoning */
		if (!SUBZONING) {
			sub.count = 0;
		} else {
			sub.count = 1;
		}
		sub.totaltime = Dt;

	} else { /** core timestep only **/
		sub.count++;
		sub.totaltime += Dt;

		if (sub.count == sub.FACTOR) {	/* last round, so do a FULL time step. */
			sub.count = 0;
		}
	} /******* end setup sub time step **************/

}

void set_velocities(void){
	/* set velocities a la Stodolkiewicz to be able to conserve energy */
	double vold2, vnew2, Unewrold, Unewrnew;
	double vt, vr;
	double Eexcess, exc_ratio;
	long i, k;
	
	k = 0;
	Eexcess = 0.0;
	for (i = 1; i <= clus.N_MAX; i++) {
		/* modify velocities of stars that have only undergone relaxation */
		if (star[i].interacted == 0) {
			Unewrold = potential(star[i].rOld) + PHI_S(star[i].rOld, i);
			Unewrnew = star[i].phi + PHI_S(star[i].r, i);
			vold2 = star[i].vtold*star[i].vtold + 
				star[i].vrold*star[i].vrold;
			vnew2 = vold2 + star[i].Uoldrold + Unewrold
				- star[i].Uoldrnew - Unewrnew;
			vt = star[i].vtold * star[i].rOld / star[i].rnew;
			if( vnew2 < vt*vt ) {
//			printf("*** OOOPSSS, trouble...\n");
				k++;
				vr = 0.0;
//			printf("% 7ld %11.4e %11.4e %11.4e\n" , 
//					i, star[i].vtold, star[i].vrold, vold2);
//			printf("        %11.4e %11.4e\n", 
//					star[i].Uoldrold, Unewrold);
//			printf("        %11.4e %11.4e %11.4e\n", 
//					star[i].Uoldrnew, Unewrnew, vnew2);
				/* by setting vr to 0, we add this much energy 
				   to the system */
				Eexcess += 0.5*(vt*vt-vnew2)*star[i].m;
			} else {
				/* choose randomized sign for vr, based on how vr was already
				   randomized in get_positions() -- don't want to waste random
				   numbers! */
				if (star[i].vr >= 0.0) {
					vr = sqrt(vnew2-vt*vt);
				} else {
					vr = -sqrt(vnew2-vt*vt);
				}
				/* if there is excess energy added, try to remove at 
				   least part of it from this star */
				if(Eexcess > 0){
					if(Eexcess < 0.5*(vt*vt+vr*vr)*star[i].m){
						exc_ratio = 
							sqrt((vt*vt+vr*vr-2*Eexcess/star[i].m)/
							     (vt*vt+vr*vr));
						vt *= exc_ratio;
						vr *= exc_ratio;
						Eexcess = 0.0;
					}
				}
			}
			star[i].vt = vt;
			star[i].vr = vr;
		}
	}
		
	/* keep track of the energy that's vanishing due to our negligence */
	Eoops += -Eexcess * madhoc;

//	if(k>0){
//		printf("** DAMN ! %ld out of %ld cases of TROUBLE!! **\n",
//				k, clus.N_MAX);
//	}
}

void set_velocities3(void){
	/* set velocities a la Stodolkiewicz to be able to conserve energy */
	double vold2, vnew2, Unewrold, Unewrnew;
	double Eexcess, exc_ratio;
	double q=0.5; /* q=0.5 -> Stodolkiewicz, q=0 -> Delta U all from rnew */
	double alpha;
	long i;
	
	Eexcess = 0.0;
	for (i = 1; i <= clus.N_MAX; i++) {
		/* modify velocities of stars that have only undergone relaxation */
		if (star[i].interacted == 0) {
			Unewrold = potential(star[i].rOld) + PHI_S(star[i].rOld, i);
			Unewrnew = star[i].phi + PHI_S(star[i].r, i);
			vold2 = star[i].vtold*star[i].vtold + 
				star[i].vrold*star[i].vrold;
			/* predict new velocity */
			vnew2 = vold2 + 2.0*(1.0-q)*(star[i].Uoldrold - star[i].Uoldrnew)
				+ 2.0*q*(Unewrold - Unewrnew);
			/* new velocity can be unphysical, so just use value predicted by old potential
			   (this is already set in .vr and .vt) */
			if (vnew2 <= 0.0) {
				Eexcess += 0.5*(sqr(star[i].vr)+sqr(star[i].vt)-vnew2)*star[i].m;
			} else {
				/* scale velocity, preserving v_t/v_r */
				alpha = sqrt(vnew2/(sqr(star[i].vr)+sqr(star[i].vt)));
				star[i].vr *= alpha;
				star[i].vt *= alpha;
				
				/* if there is excess energy added, try to remove at 
				   least part of it from this star */
				if(Eexcess > 0 && Eexcess < 0.5*(sqr(star[i].vt)+sqr(star[i].vr))*star[i].m){
					exc_ratio = 
						sqrt( (sqr(star[i].vt)+sqr(star[i].vr)-2*Eexcess/star[i].m)/
						      (sqr(star[i].vt)+sqr(star[i].vr)) );
					star[i].vr *= exc_ratio;
					star[i].vt *= exc_ratio;
					Eexcess = 0.0;
				}
			}
		}
	}
	
	/* keep track of the energy that's vanishing due to our negligence */
	Eoops += -Eexcess * madhoc;
}

void RecomputeEnergy(void) {
	double dtemp;
	long i, neligible=0, ntrustvr=0, nstodol=0, nsplit=0, ntransfer=0;

	/* Recalculating Energies */
	Etotal.tot = 0.0;
	Etotal.K = 0.0;
	Etotal.P = 0.0;
	Etotal.Eint = 0.0;
	Etotal.Eb = 0.0;
	
	/* Kris Joshi: Try to conserve energy by using intermediate potential */
	/* John Fregeau: This is an obfuscated and incorrect way of applying 
	   the scheme of Stodolkiewicz (1982) to better conserve energy.  See eq. (33) 
	   of that paper.  Here are the important variables:
	   
	   r_old, vr_old, vt_old = values taken after dynamics_apply() but before get_positions()
	   r, vr, vt = values taken after get_positions(), these are "predicted" values based 
	               on old potential
	   r_new=r, vr_new, vt_new = values calculated based on improved energy conservation scheme
	                             of Stodolkiewicz (1982)
	   Phi_old = the potential before it is updated
	   Phi = the "predicted" potential after it is updated
	   EI = v_old^2 + Phi_old(r_old) - Phi_old(r)
	   dtemp = v_old^2 + Phi_old(r_old) - Phi_old(r) - Phi(r) + Phi(r_old) = v_new^2
	*/
	
	if (E_CONS==1) {
		for (i=1; i<=clus.N_MAX; i++) {
			/* Kris Joshi: Note: svt[] = J/r_new is already computed in get_positions() */
			/* John Fregeau: In other words, vt_new = vt_old * r_old / r_new, as specified 
			   in eq. (34) of Stodolkiewicz (1982) (as long as J == vt_old * r_old, which I'm
			   not exactly sure of). */
			/* Kris Joshi: ignore stars near pericenter, and those with strong interactions */
			if (star[i].X > 0.05 && star[i].interacted == 0) {
				/* count up number of stars on which we could have applied the energy correction technique */
				neligible++;
				/* John Fregeau: dtemp = v_new^2; see eq. (33) of Stodolkiewicz (1982) */
				dtemp = star[i].EI - star[i].phi + potential(star[i].rOld);

				if (dtemp > sqr(star[i].vr)) {
					ntrustvr++;
					/* Kris Joshi: preserve star[i].vr and change star[i].vt */
					/* John Fregeau: I guess for some reason one assumes that if v_new^2 > vr_predicted^2,
					   then we should trust vr.  I don't see why this should be true. */
					star[i].vt = sqrt(dtemp - star[i].vr * star[i].vr);
				} else {
					if (dtemp > 0) {
						if (dtemp > star[i].vt * star[i].vt) {
							nstodol++;
							/* John Fregeau: this is the standard Stodolkiewicz scheme */
							/* choose randomized sign for vr, based on how vr was already
							   randomized in get_positions() -- don't want to waste random
							   numbers! */
							if (star[i].vr >= 0.0) {
								star[i].vr = sqrt(dtemp - sqr(star[i].vt));
							} else {
								star[i].vr = -sqrt(dtemp - sqr(star[i].vt));
							}
						} else {
							nsplit++;
							/* John Fregeau: just arbitrarily split the kinetic energy equally
							   into radial and tangential?!? */
							star[i].vt = sqrt(dtemp / 2.0);
							star[i].vr = sqrt(dtemp / 2.0);
						}
					} else {
						/* Kris Joshi: reduce the energy of the next star to compensate */ 
						/* John Fregeau: ad hoc energy transfer from star to star to conserve
						   energy. */
						if (i < clus.N_MAX) {
							ntransfer++;
							star[i + 1].EI += dtemp - (sqr(star[i].vt) + sqr(star[i].vr));
						}
					}
				}
			}
		}
	}

	/* report statistics on Stodolkiewicz scheme */
	/* dprintf("Stodolkiewicz energy scheme: neligible=%ld of %ld ntrustvr=%.3g%% nstodol=%.3g%% nsplit=%.3g%% ntransfer=%.3g%%\n",
		neligible, clus.N_MAX, 100.0*((double) ntrustvr)/((double) neligible), 100.0*((double) nstodol)/((double) neligible), 
		100.0*((double) nsplit)/((double) neligible), 100.0*((double) ntransfer)/((double) neligible)); */

	/* calculate energies */
	for (i=1; i<=clus.N_MAX; i++) {
		star[i].E = star[i].phi + 0.5 * (sqr(star[i].vr) + sqr(star[i].vt));
		star[i].J = star[i].r * star[i].vt;
		
		Etotal.K += 0.5 * (sqr(star[i].vr) + sqr(star[i].vt)) * star[i].m / clus.N_STAR;
		
		/* Compute PE using Henon method using star[].phi */
		Etotal.P += star[i].phi * star[i].m / clus.N_STAR;

		if (star[i].binind == 0) {
			Etotal.Eint += star[i].Eint;
		} else {
			if (binary[star[i].binind].inuse){
				Etotal.Eb += -(binary[star[i].binind].m1/clus.N_STAR) * (binary[star[i].binind].m2/clus.N_STAR) / 
					(2.0 * binary[star[i].binind].a);
				Etotal.Eint += binary[star[i].binind].Eint1 + binary[star[i].binind].Eint2;
			}
		}
	}

	Etotal.P *= 0.5;
	Etotal.tot = Etotal.K + Etotal.P + Etotal.Eint + Etotal.Eb + cenma.E/clus.N_STAR + Eescaped + Ebescaped + Eintescaped;
}

/* computes intermediate energies, and transfers "new" dynamical params to the standard variables */
void ComputeIntermediateEnergy(void)
{
	long j;

	/* compute intermediate energies for stars due to change in pot */ 
	for (j = 1; j <= clus.N_MAX_NEW; j++) {
		/* but do only for NON-Escaped stars */
		if (star[j].rnew < 1.0e6) {
			star[j].EI = sqr(star[j].vr) + sqr(star[j].vt) + star[j].phi - potential(star[j].rnew);
		}
	}
	
	/* Transferring new positions to .r, .vr, and .vt from .rnew, .vrnew, and .vtnew */
	for (j = 1; j <= clus.N_MAX_NEW; j++) {
		star[j].rOld = star[j].r;
		star[j].r = star[j].rnew;
		star[j].vr = star[j].vrnew;
		star[j].vt = star[j].vtnew;
	}
}

long CheckStop(struct tms tmsbufref) {
	struct tms tmsbuf;
	long tspent;

	times(&tmsbuf);
	tspent  = tmsbuf.tms_utime-tmsbufref.tms_utime;
	tspent += tmsbuf.tms_stime-tmsbufref.tms_stime;
	tspent += tmsbuf.tms_cutime-tmsbufref.tms_cutime;
	tspent += tmsbuf.tms_cstime-tmsbufref.tms_cstime;
	tspent /= sysconf(_SC_CLK_TCK);
	tspent /= 60;

	if (tspent >= MAX_WCLOCK_TIME) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("MAX_WCLOCK_TIME exceeded ... Terminating.\n");
		return (1);
	}
	
	if (tcount >= T_MAX_COUNT) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("No. of timesteps > T_MAX_COUNT ... Terminating.\n");
		return (1);
	}

	if (TotalTime >= T_MAX) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("TotalTime > T_MAX ... Terminating.\n");
		return (1);
	}

	/* Stop if cluster is disrupted -- N_MAX is too small */
	/* if (clus.N_MAX < (0.02 * clus.N_STAR)) { */
	if (clus.N_MAX < (0.005 * clus.N_STAR)) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("N_MAX < 0.005 * N_STAR ... Terminating.\n");
		return (1);
	}

	/* Stop if Etotal > 0 */
	if (Etotal.K + Etotal.P > 0.0) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("Etotal > 0 ... Terminating.\n");
		return (1);
	}


	/* If inner-most Lagrangian radius is too small, then stop: */
	if (mass_r[0] < MIN_LAGRANGIAN_RADIUS) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("Min Lagrange radius < %.6G ... Terminating.\n", MIN_LAGRANGIAN_RADIUS);
		return (1);
	}

	/* Output some snapshots near core collapse 
	 * (if core density is high enough) */
	if (SNAPSHOT_PERIOD){
		if (rho_core > 50.0 && Echeck == 0) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 1.0e2 && Echeck == 1) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 5.0e2 && Echeck == 2) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 1.0e3 && Echeck == 3) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 5.0e3 && Echeck == 4) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 1.0e4 && Echeck == 5) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 5.0e4 && Echeck == 6) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 1.0e5 && Echeck == 7) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 5.0e5 && Echeck == 8) {
			print_2Dsnapshot();
			Echeck++;
		} else if (rho_core > 1.0e6 && Echeck == 9) {
			print_2Dsnapshot();
			Echeck++;
		}

		/* added by ato 
		 * to try to take snapshots for core bounce as well. 
		 * idea is if we reduced core density by 10 percent the
		 * last time we took snapshot, take another one and adjust
		 * parameters to take further snapshots if further collapse
		 * occurs */
		if (rho_core < 0.9e6 && Echeck == 10){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9*5e5 && Echeck == 9){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9e5 && Echeck == 8){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9*5e4 && Echeck == 7){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9e4 && Echeck == 6){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9*5e3 && Echeck == 5){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9e3 && Echeck == 4){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9*5e2 && Echeck == 3){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9e2 && Echeck == 2){
			print_2Dsnapshot();
			Echeck--;
		} else if (rho_core < 0.9*50 && Echeck == 1){
			print_2Dsnapshot();
			Echeck--;
		} 
	}

	/* If total Energy has diminished by TERMINAL_ENERGY_DISPLACEMENT, then stop */
	if (Etotal.tot < Etotal.ini - TERMINAL_ENERGY_DISPLACEMENT) {
		if (SNAPSHOT_PERIOD)
			print_2Dsnapshot();
		diaprintf("Terminal Energy reached... Terminating.\n");
		return (1);
	}
	return (0); /* NOT stopping time yet */
}

/* energy calculation function that is called for a restart (so it's not necessary to 
   re-set all global energy variables */
void ComputeEnergy2(void)
{
	long k, i;

	for (i = 1; i <= clus.N_MAX; i++) {
		k = i;
		star[k].E = star[k].phi + 0.5 * (star[k].vr * star[k].vr + star[k].vt * star[k].vt);

		star[k].J = star[k].r * star[k].vt;
	}
	
	fprintf(stdout, "Time = %.8G   Tcount = %ld\n", TotalTime, tcount);
	fprintf(stdout, "N = %ld, Total E = %.8G, Total Mass = %.8G, Virial ratio = %.8G\n",
		clus.N_MAX, Etotal.tot, Mtotal, -2.0 * Etotal.K / Etotal.P);
	fprintf(stdout, "Total KE = %.8G, Total PE = %.8G\n", Etotal.K, Etotal.P);
}

void ComputeEnergy(void)
{
	long k, i;

	Etotal.tot = 0.0;
	Etotal.K = 0.0;
	Etotal.P = 0.0;
	Etotal.Eint = 0.0;
	Etotal.Eb = 0.0;

	star[0].E = star[0].J = 0.0;
	for (i = 1; i <= clus.N_MAX; i++) {
		k = i;
		star[k].E = star[k].phi + 0.5 * (star[k].vr * star[k].vr + star[k].vt * star[k].vt);

		star[k].J = star[k].r * star[k].vt;

		Etotal.K += 0.5 * (star[k].vr * star[k].vr + star[k].vt * star[k].vt) * star[k].m / clus.N_STAR;

		Etotal.P += star[k].phi * star[k].m / clus.N_STAR;

		if (star[k].binind == 0) {
			Etotal.Eint += star[k].Eint;
		} else if (binary[star[k].binind].inuse) {
			Etotal.Eb += -(binary[star[k].binind].m1/clus.N_STAR) * (binary[star[k].binind].m2/clus.N_STAR) / 
						(2.0 * binary[star[k].binind].a);
			Etotal.Eint += binary[star[k].binind].Eint1 + binary[star[k].binind].Eint2;
		}
	}
	star[clus.N_MAX+1].E = star[clus.N_MAX+1].J = 0.0;
	
	Etotal.P *= 0.5;
	Etotal.tot = Etotal.K + Etotal.P + Etotal.Eint + Etotal.Eb + cenma.E/clus.N_STAR + Eescaped + Ebescaped + Eintescaped;

	fprintf(stdout, "Time = %.8G   Tcount = %ld\n", TotalTime, tcount);
	fprintf(stdout, "N = %ld, Total E = %.8G, Total Mass = %.8G, Virial ratio = %.8G\n",
		clus.N_MAX, Etotal.tot, Mtotal, -2.0 * Etotal.K / Etotal.P);
	fprintf(stdout, "Total KE = %.8G, Total PE = %.8G\n", Etotal.K, Etotal.P);
}

/* Computing the potential at each star sorted by increasing 
   radius. Units: G = 1  and  Mass is in units of total INITIAL mass.
   Total mass is computed by SUMMING over all stars that have NOT ESCAPED 
   i.e., over all stars upto N_MAX <= N_STAR. N_MAX is computed in this 
   routine by counting all stars with radius < SF_INFINITY and Radius of the 
   (N_MAX+1)th star is set to infinity i.e., star[N_MAX+1].r = 
   SF_INFINITY. Also setting star[N_MAX+1].phi = 0. Assuming 
   star[0].r = 0. star[].phi is also indexed i.e. it
   is the value of the potential at radius star[k].r 
   NOTE: Assming here that NO two stars are at the SAME RADIUS upto 
   double precision. Returns N_MAX. Potential given in star[].phi
*/
long potential_calculate(void) {
	long k;
	double mprev;
	static int firstcall=1;
	static double M, KE, a;

	/* calculate Plummer parameters on first function call */
	if (firstcall) {
		firstcall = 0;
		M = 0.0;
		KE = 0.0;
		for (k=1; k<=clus.N_STAR; k++) {
			M += star[k].m/clus.N_STAR;
			KE += 0.5 * star[k].m/clus.N_STAR * (sqr(star[k].vr)+sqr(star[k].vt));
		}
		a = (3.0*PI/64.0) * sqr(M) / KE;
	}

	/* count up all the mass and set N_MAX */
	k = 1;
	mprev = 0.0;
	while (star[k].r < SF_INFINITY && k <= clus.N_STAR_NEW) {
		mprev += star[k].m;
		/* I guess NaNs do happen... */
		if(isnan(mprev)){
			eprintf("NaN (2) detected\n");
			exit_cleanly(-1);
		}
		k++;
	}

	/* New N_MAX */
	clus.N_MAX = k - 1;

	/* New total Mass; This IS correct for multiple components */
	Mtotal = mprev/clus.N_STAR + cenma.m/clus.N_STAR;	

	/* Compute new tidal radius using new Mtotal */

	Rtidal = orbit_r * pow(Mtotal, 1.0 / 3.0);

	/* zero boundary star first for safety */
	zero_star(clus.N_MAX + 1);

	star[clus.N_MAX + 1].r = SF_INFINITY;
	star[clus.N_MAX + 1].phi = 0.0;

	mprev = Mtotal;
	for (k = clus.N_MAX; k >= 1; k--) {/* Recompute potential at each r */
		star[k].phi = star[k + 1].phi - mprev * (1.0 / star[k].r - 1.0 / star[k + 1].r);
		mprev -= star[k].m / clus.N_STAR;
		/* explicitly do Plummer potential */
		/* star[k].phi = -(M/a) / sqrt(1.0 + sqr(star[k].r/a)); */
	}

	for (k = 1; k <= clus.N_MAX; k++){
		star[k].phi -= cenma.m / clus.N_STAR / star[k].r;
		if(isnan(star[k].phi)){
			eprintf("NaN detected\n");
			exit_cleanly(-1);
		}
	}
	
	star[0].phi = star[1].phi; /* U(r=0) is U_1 */

	return (clus.N_MAX);
}

#define GENSEARCH_NAME 				m_binsearch
#define GENSEARCH_TYPE 				double
#define GENSEARCH_KEYTYPE			double
#define GENSEARCH_GETKEY(a)			a
#define GENSEARCH_COMPAREKEYS(k1, k2)	k1 < k2

#include "gensearch.h"

int find_stars_mass_bin(double smass){
	/* find the star[i]'s mass bin */
	/* return -1 on failure */
	int bn;

	if ( (smass < mass_bins[0]) || 
	     (smass > mass_bins[NO_MASS_BINS-1]) ) return -1;
	
	bn = m_binsearch(mass_bins, 0, NO_MASS_BINS-1, smass);
	return bn;
}

void comp_multi_mass_percent(){
	/* computing the Lagrange radii for various mass bins */
	/* mass bins are stored in the array mass_bins[NO_MASS_BINS] */
	double *mtotal_inbin; // total mass in each mass bin
	long *number_inbin;   // # of stars in each mass bin
	double *mcount_inbin, *r_inbin;
	long *ncount_inbin;
	int *star_bins;       // array holding which bin each star is in
	double **rs, **percents;
	long i;

	/* GSL interpolation function and accelerators. See:
	 * http://sources.redhat.com/gsl/ref/gsl-ref_26.html#SEC391*/
	gsl_interp_accel **acc;
	gsl_spline **spline;

	if (NO_MASS_BINS <=1) return;

	mtotal_inbin = calloc(NO_MASS_BINS, sizeof(double));
	number_inbin = calloc(NO_MASS_BINS, sizeof(long));
	r_inbin = calloc(NO_MASS_BINS, sizeof(double));
	star_bins = malloc((clus.N_MAX+2)*sizeof(int));
	for (i = 1; i <= clus.N_MAX; i++) {
		star_bins[i] = find_stars_mass_bin(star[i].m/SOLAR_MASS_DYN);
		if (star_bins[i] == -1) continue; /* -1: star isn't in legal bin */
		mtotal_inbin[star_bins[i]] += star[i].m; /* no unit problem, since 
								        we are interested in
							   	        percentage only. */
		number_inbin[star_bins[i]]++;
		r_inbin[star_bins[i]] = star[i].r;
	}
	/* populate arrays rs[NO_MASS_BINS][j] and percents[][] */
	rs = malloc(NO_MASS_BINS*sizeof(double *));
	percents = malloc(NO_MASS_BINS*sizeof(double *));
	for(i=0; i<NO_MASS_BINS; i++){
		/* +1 below is to accomodate rs=0 <-> percents=0 point */
		rs[i] = malloc((number_inbin[i]+1)*sizeof(double));
		percents[i] = malloc((number_inbin[i]+1)*sizeof(double));
	}
	for(i=0; i<NO_MASS_BINS; i++){
		/* at r=0 there is 0% of mass */
		rs[i][0] = percents[i][0] = 0.0;
	}
	
	mcount_inbin = calloc(NO_MASS_BINS, sizeof(double));
	ncount_inbin = calloc(NO_MASS_BINS, sizeof(long));
	for (i = 1; i <= clus.N_MAX; i++) {
		int sbin = star_bins[i];
		if (sbin == -1) continue;
		mcount_inbin[sbin] += star[i].m;
		ncount_inbin[sbin]++;
		rs[sbin][ncount_inbin[sbin]] = star[i].r;
		percents[sbin][ncount_inbin[sbin]] = 
				mcount_inbin[sbin]/mtotal_inbin[sbin];
	}
	free(mcount_inbin); free(ncount_inbin);
	
	acc = malloc(NO_MASS_BINS*sizeof(gsl_interp_accel));
	spline = malloc(NO_MASS_BINS*sizeof(gsl_spline));
	for(i=0; i<NO_MASS_BINS; i++){
		if((number_inbin[i] == 1) || (number_inbin[i] == 0)) continue;
		acc[i] = gsl_interp_accel_alloc();
		/* change gsl_interp_linear to gsl_interp_cspline below,
		 * for spline interpolation; however, this may result in
		 * negative LR values for small mass percentages! */ 
		/* +1's below are to accomodate rs=0 <-> percents=0 point */
		spline[i] = gsl_spline_alloc(gsl_interp_linear, number_inbin[i]+1);
		gsl_spline_init (spline[i], percents[i], rs[i], number_inbin[i]+1);
	}
	for(i=0; i<NO_MASS_BINS; i++){
		free(rs[i]); free(percents[i]);
	}
	free(rs); free(percents);
	
	/* fill multi_mass_r[][] by calling gsl_spline_eval() */
	for (i = 0; i <NO_MASS_BINS; i++) {
		int mcnt;
		if (number_inbin[i] == 0) {
			continue;
		} else if (number_inbin[i] == 1) {
			for(mcnt=0; mcnt<MASS_PC_COUNT; mcnt++){
				multi_mass_r[i][mcnt] = r_inbin[i];
			}
		} else {
			for(mcnt=0; mcnt<MASS_PC_COUNT; mcnt++){
				multi_mass_r[i][mcnt] = 
					gsl_spline_eval(spline[i], mass_pc[mcnt], acc[i]);
			}
		}
	}
	for(i=0; i<NO_MASS_BINS; i++){
		if((number_inbin[i] == 1) || (number_inbin[i] == 0)) continue;
		gsl_interp_accel_free(acc[i]); 
		gsl_spline_free(spline[i]);
	}
	free(acc); free(spline);
	
	free(mtotal_inbin); free(number_inbin); free(r_inbin); 
	free(star_bins);
}
		
void comp_mass_percent(){
	double mprev;
	long int k, mcount;

	/* Computing radii containing mass_pc[i] % of the mass */
	mprev = cenma.m/clus.N_STAR;
	for(mcount=0; mcount<MASS_PC_COUNT; mcount++){
		if ( mprev/Mtotal > mass_pc[mcount] ) {
			mass_r[mcount] = MINIMUM_R;
			ave_mass_r[mcount] = 0.0;
			no_star_r[mcount] = 0;
			densities_r[mcount] = 0.0;
		} else {
			break;
		}
	}
	for (k = 1; k <= clus.N_MAX; k++) {	/* Only need to count up to N_MAX */
		mprev += star[k].m / clus.N_STAR;
		if (mprev / Mtotal > mass_pc[mcount]) {
			mass_r[mcount] = star[k].r;
			ave_mass_r[mcount] = mprev/Mtotal/k*initial_total_mass;
			no_star_r[mcount] = k;
			densities_r[mcount] = mprev*clus.N_STAR/
				(4/3*3.1416*pow(star[k].r,3));
			mcount++;
			if (mcount == MASS_PC_COUNT)
				break;
		}
	}
}

/* The potential computed using the star[].phi computed at the star 
   locations in star[].r sorted by increasing r. */
double potential(double r) {
	long i;
	double henon;

	/* root finding using indexed values of sr[] & bisection */
	if (r < star[1].r)
		return (star[1].phi);

	i =  FindZero_r(1, clus.N_MAX + 1, r);

	if(star[i].r > r || star[i+1].r < r){
		eprintf("binary search (FindZero_r) failed!!\n");
		eprintf("pars: i=%ld, star[i].r = %e, star[i+1].r = %e, star[i+2].r = %e, star[i+3].r = %e, r = %e\n",
				i, star[i].r, star[i+1].r, star[i+2].r, star[i+3].r, r);
		eprintf("pars: star[i].m=%g star[i+1].m=%g star[i+2].m=%g star[i+3].m=%g\n",
			star[i].m, star[i+1].m, star[i+2].m, star[i+3].m);
		exit_cleanly(-2);
	}

	/* Henon's method of computing the potential using star[].phi */ 
	if (i == 0){ /* I think this is impossible, due to early return earlier,
			    but I am keeping it. -- ato 23:17,  3 Jan 2005 (UTC) */
		henon = (star[1].phi);
	} else {
		henon = (star[i].phi + (star[i + 1].phi - star[i].phi) 
			 * (1.0/star[i].r - 1.0/r) /
			 (1.0/star[i].r - 1.0/star[i + 1].r));
	}
	
	return (henon);
}

/*****************************************/
/* Unmodified Numerical Recipes Routines */
/*****************************************/
#define NR_END 1
#define FREE_ARG char*

void nrerror(char error_text[])
/* Numerical Recipes standard error handler */
{
	fprintf(stderr,"Numerical Recipes run-time error...\n");
	fprintf(stderr,"%s\n",error_text);
	fprintf(stderr,"...now exiting to system...\n");
	exit_cleanly(1);
}

double *vector(long nl, long nh)
/* allocate a double vector with subscript range v[nl..nh] */
{
	double *v;

	v=(double *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(double)));
	if (!v) nrerror("allocation failure in vector()");
	return v-nl+NR_END;
}

int *ivector(long nl, long nh)
/* allocate an int vector with subscript range v[nl..nh] */
{
	int *v;

	v=(int *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(int)));
	if (!v) nrerror("allocation failure in ivector()");
	return v-nl+NR_END;
}

void free_vector(double *v, long nl, long nh)
/* free a double vector allocated with vector() */
{
	free((FREE_ARG) (v+nl-NR_END));
}

void free_ivector(int *v, long nl, long nh)
/* free an int vector allocated with ivector() */
{
	free((FREE_ARG) (v+nl-NR_END));
}

#undef NR_END
#undef FREE_ARG

/* update some important global variables */
void update_vars(void)
{
	long i, j, k;
	
	/* update total number, mass, and binding energy of binaries in cluster */
	N_b = 0;
	M_b = 0.0;
	E_b = 0.0;
	for (i=1; i<=clus.N_MAX; i++) {
		j = i;
		k = star[j].binind;
		if (k != 0) {
			N_b++;
			M_b += star[j].m;
			if (binary[k].inuse){
				E_b += (binary[k].m1/clus.N_STAR) * (binary[k].m2/clus.N_STAR) / (2.0 * binary[k].a);
			}
		}
	}
}

void mini_sshot(){
	FILE *mss;
	char *mss_fname;
	int fname_len;
	int i;

	fname_len = strlen(outprefix);
	fname_len += strlen("miniss.");
	fname_len += 10;
	mss_fname = malloc(fname_len*sizeof(char));
	sprintf(mss_fname,"%s_miniss.%05ld", outprefix, tcount);
	mss = fopen(mss_fname, "w+");
	for(i=0; i<1000; i++){
		fprintf(mss, "%8ld %.16e %.16e %.16e %.16e ", 
				star[i].id, star[i].m, star[i].r_peri, 
				star[i].r, star[i].r_apo);
		fprintf(mss, "%.16e %.16e %.16e %.16e\n", 
				star[i].vr, star[i].vt, star[i].E, star[i].phi);
	}
	fclose(mss);
}

/* set the units */
void units_set(void)
{
	/* define (N-body) units (in CGS here): U_l = U_t^(2/3) G^(1/3) U_m^(1/3) */
	units.t = log(GAMMA * clus.N_STAR)/(clus.N_STAR * MEGA_YEAR) * 1.0e6 * YEAR;
	units.m = clus.N_STAR * initial_total_mass / SOLAR_MASS_DYN * MSUN;
	units.l = pow(units.t, 2.0/3.0) * pow(G, 1.0/3.0) * pow(units.m, 1.0/3.0);
	units.E = G * sqr(units.m) / units.l;
	/* stars' masses are kept in different units */
	units.mstar = initial_total_mass / SOLAR_MASS_DYN * MSUN;

	/* Masses such as star.m and binary.m1 and binary.m2 are not stored in code units, 
	   but rather in code units * clus.N_STAR.  This means that whenever you want to
	   calculate a quantity that involves masses and lengths, or masses and times, you
	   have to divide any masses in the expression by clus.N_STAR.  This is that 
	   factor.
	*/
	madhoc = 1.0/((double) clus.N_STAR);

	/* print out diagnostic information */
	diaprintf("MEGA_YEAR=%g\n", MEGA_YEAR);
	diaprintf("SOLAR_MASS_DYN=%g\n", SOLAR_MASS_DYN);
	diaprintf("initial_total_mass=%g\n", initial_total_mass);
	diaprintf("units.t=%g YEAR\n", units.t/YEAR);
	diaprintf("units.m=%g MSUN\n", units.m/MSUN);
	diaprintf("units.mstar=%g MSUN\n", units.mstar/MSUN);
	diaprintf("units.l=%g PARSEC\n", units.l/PARSEC);
	diaprintf("units.E=%g erg\n", units.E);
	diaprintf("t_rel=%g YEAR\n", units.t * clus.N_STAR / log(GAMMA * clus.N_STAR) / YEAR);
}

/* calculate central quantities */
void central_calculate(void)
{
	long i;

	central.N = 0;
	central.N_sin = 0;
	central.N_bin = 0;

	central.M = 0.0;
	central.M_sin = 0.0;
	central.M_bin = 0.0;

	central.v_rms = 0.0;
	central.v_sin_rms = 0.0;
	central.v_bin_rms = 0.0;

	central.w2_ave = 0.0;
	central.R2_ave = 0.0;
	central.mR_ave = 0.0;

	central.a_ave = 0.0;
	central.a2_ave = 0.0;
	central.ma_ave = 0.0;

	for (i=1; i<=MIN(NUM_CENTRAL_STARS,clus.N_STAR); i++) {
		central.N++;
		/* use only code units here, so always divide star[].m by clus.N_STAR */
		central.M += star[i].m / ((double) clus.N_STAR);
		central.v_rms += sqr(star[i].vr) + sqr(star[i].vt);
		central.w2_ave += 2.0 * star[i].m / ((double) clus.N_STAR) * (sqr(star[i].vr) + sqr(star[i].vt));

		if (star[i].binind == 0) {
			central.N_sin++;
			central.M_sin += star[i].m / ((double) clus.N_STAR);
			central.v_sin_rms += sqr(star[i].vr) + sqr(star[i].vt);
			central.R2_ave += sqr(star[i].rad);
			central.mR_ave += star[i].m / ((double) clus.N_STAR) * star[i].rad;
		} else {
			central.N_bin++;
			central.M_bin += star[i].m / ((double) clus.N_STAR);
			central.v_bin_rms += sqr(star[i].vr) + sqr(star[i].vt);
			central.a_ave += binary[star[i].binind].a;
			central.a2_ave += sqr(binary[star[i].binind].a);
			central.ma_ave += star[i].m / ((double) clus.N_STAR) * binary[star[i].binind].a;
		}
	}
	/* object quantities */
	central.r = star[central.N + 1].r;
	central.V = 4.0/3.0 * PI * cub(central.r);
	central.n = ((double) central.N) / central.V;
	central.rho = central.M / central.V;
	central.m_ave = central.M / ((double) central.N);
	central.v_rms = sqrt(central.v_rms / ((double) central.N));
	central.w2_ave /= central.m_ave * ((double) central.N);
	
	/* single star quantities */
	central.n_sin = ((double) central.N_sin) / central.V;
	central.rho_sin = central.M_sin / central.V;
	if (central.N_sin != 0) {
		central.m_sin_ave = central.M_sin / ((double) central.N_sin);
		central.v_sin_rms = sqrt(central.v_sin_rms / ((double) central.N_sin));
		central.R2_ave /= ((double) central.N_sin);
		central.mR_ave /= ((double) central.N_sin);
	} else {
		central.m_sin_ave = 0.0;
		central.v_sin_rms = 0.0;
		central.R2_ave = 0.0;
		central.mR_ave = 0.0;
	}
	
	/* binary star quantities */
	central.n_bin = ((double) central.N_bin) / central.V;
	central.rho_bin = central.M_bin / central.V;
	if (central.N_bin != 0) {
		central.m_bin_ave = central.M_bin / ((double) central.N_bin);
		central.v_bin_rms = sqrt(central.v_bin_rms / ((double) central.N_bin));
		central.a_ave /= ((double) central.N_bin);
		central.a2_ave /= ((double) central.N_bin);
		central.ma_ave /= ((double) central.N_bin);
	} else {
		central.m_bin_ave = 0.0;
		central.v_bin_rms = 0.0;
		central.a_ave = 0.0;
		central.a2_ave = 0.0;
		central.ma_ave = 0.0;
	}

	/* set global variables that are used throughout the code */
	v_core = central.v_rms;
	rho_core = central.rho;
	rho_core_single = central.rho_sin;
	rho_core_bin = central.rho_bin;
	core_radius = sqrt(3.0 * sqr(central.v_rms) / (4.0 * PI * central.rho));
	/* Kris Joshi's original expression was 
	   N_core = 2.0 / 3.0 * PI * cub(core_radius) * (1.0 * clus.N_STAR) * central.rho; */
	N_core = 4.0 / 3.0 * PI * cub(core_radius) * central.n;
	/* core relaxation time, Spitzer (1987) eq. (2-62) */
	Trc = 0.065 * cub(central.v_rms) / (central.rho * central.M);
}
