#include <bmx.H>

#include <AMReX_VisMF.H>
#include <bmx_mf_helpers.H>
#include <bmx_dem_parms.H>
#include <bmx_fluid_parms.H>
#include <bmx_chem_species_parms.H>

void
bmx::EvolveFluid (int nstep,
                   Real& dt,
                   Real& prev_dt,
                   Real& time,
                   Real stop_time,
                   Real& coupling_timing)
{
    BL_PROFILE_REGION_START("bmx::EvolveFluid");
    BL_PROFILE("bmx::EvolveFluid");

    amrex::Print() << "\n ============   NEW TIME STEP   ============ \n";

    // Extrapolate boundary values 
    for (int lev = 0; lev <= finest_level; lev++)
    {
        m_leveldata[lev]->D_gk->FillBoundary(geom[lev].periodicity());
        m_leveldata[lev]->X_gk->FillBoundary(geom[lev].periodicity());
    }

    bmx_set_chem_species_bcs(time, get_X_gk(), get_D_gk());

    // Create temporary multifabs to hold the old-time lap_X and vel_RHS
    //    so we don't have to re-compute them in the corrector
    Vector< MultiFab* > lap_X(finest_level+1);
    Vector< MultiFab* > chem_species_RHS(finest_level+1);

    const int nchem_species_g = FLUID::nchem_species;

    for (int lev = 0; lev <= finest_level; lev++)
    {
       lap_X[lev]       = new MultiFab(grids[lev], dmap[lev], nchem_species_g, 0, MFInfo());
       chem_species_RHS[lev] = new MultiFab(grids[lev], dmap[lev], nchem_species_g, 0, MFInfo());
    }

    dt = fixed_dt;

    // Set new and old time to correctly use in fillpatching
    for (int lev = 0; lev <= finest_level; lev++)
    {
        t_old[lev] = time;
        t_new[lev] = time+dt;
    }

    amrex::Print() << "\n   Step " << nstep+1 << ": from old_time " \
                   << time << " to new time " << time+dt
                   << " with dt = " << dt << "\n" << std::endl;

    // Copy current "new" into "old"
    for (int lev = 0; lev <= finest_level; lev++)
    {
      MultiFab& X_gk = *m_leveldata[lev]->X_gk;
      MultiFab& X_gko = *m_leveldata[lev]->X_gko;

      MultiFab::Copy(X_gko, X_gk, 0, 0, X_gk.nComp(), X_gko.nGrow());
    }

    // Interpolate chem_species to particle locations
    bmx_calc_txfr_particle(time);

    // Deposit sources/sink from individual particles to grid
    bmx_calc_txfr_fluid(time);

    //
    // Time integration step
    //
    Real new_time = time+dt;

    if (m_diff_type == DiffusionType::Implicit) amrex::Print() << "Doing fully implicit diffusion..." << std::endl;
    if (m_diff_type == DiffusionType::Explicit) amrex::Print() << "Doing fully epplicit diffusion..." << std::endl;
    if (m_diff_type == DiffusionType::Crank_Nicolson) amrex::Print() << "Doing Crank-Nicolson diffusion..." << std::endl;

    // Local flag for explicit diffusion
    bool l_explicit_diff = (m_diff_type == DiffusionType::Explicit);

    fillpatch_all(get_X_gk_old(), new_time);

    // *************************************************************************************
    // Compute explicit diffusive terms
    // *************************************************************************************

    if (m_diff_type != DiffusionType::Implicit)
        diffusion_op->ComputeLapX(lap_X, get_X_gk_old(), get_D_gk_const());

    // *************************************************************************************
    // Compute right hand side terms on the old status
    // *************************************************************************************

    // *************************************************************************
    // Update 
    // *************************************************************************
    Real l_dt = dt;

    // theta = 1 for fully explicit, theta = 1/2 for Crank-Nicolson
    Real theta;

    if (m_diff_type == DiffusionType::Explicit) theta = 0.0;
    if (m_diff_type == DiffusionType::Implicit) theta = 1.0;
    if (m_diff_type == DiffusionType::Crank_Nicolson) theta = 0.5;

    for (int lev = 0; lev <= finest_level; lev++)
    {
        auto& ld = *m_leveldata[lev];
#ifdef _OPENMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(*ld.X_gk,TilingIfNotGPU()); mfi.isValid(); ++mfi)
        {
            Box const& bx = mfi.tilebox();
            Array4<Real      > const& X_gk_n    = ld.X_gk->array(mfi);
            Array4<Real const> const& X_gk_o    = ld.X_gko->const_array(mfi);
            Array4<Real const> const& lap_X_arr = lap_X[lev]->const_array(mfi);
            Array4<Real const> const& X_RHS_arr = ld.X_rhs[lev].const_array(mfi);

            amrex::Print() << "UPDATING ON BX " << bx << std::endl;

            ParallelFor(bx, nchem_species_g, [=]
              AMREX_GPU_DEVICE (int i, int j, int k, int n) noexcept
            {
                // amrex::Print() << "OLD LAP " << IntVect(i,j,k) << " " << 
                //         X_gk_o(i,j,k,n) << " " << lap_X_arr(i,j,k,n) << std::endl;
                X_gk_n(i,j,k,n) += theta * l_dt * lap_X_arr(i,j,k,n) + l_dt * X_RHS_arr(i,j,k,n);
                if (i == 16 and j == 16 and k == 16) 
                   amrex::Print() << "OLD / NEW " << IntVect(i,j,k) << " " << 
                   X_gk_o(i,j,k,n) << " " << X_gk_n(i,j,k,n) << std::endl;
                // amrex::Print() << "SOURCE TERM " << X_RHS_arr(i,j,k,n) << std::endl;
            });
        } // mfi
    } // lev

    // *************************************************************************************
    // If doing implicit diffusion...
    // *************************************************************************************
    if (not l_explicit_diff) 
    {
        bmx_set_chem_species_bcs(time, get_X_gk(), get_D_gk());

        diffusion_op->diffuse_chem_species(get_X_gk(), get_D_gk(), theta, l_dt);
    }

    for (int lev = 0; lev <= finest_level; lev++)
    {
       delete lap_X[lev];
       delete chem_species_RHS[lev];
    }

    BL_PROFILE_REGION_STOP("bmx::EvolveFluid");
}
