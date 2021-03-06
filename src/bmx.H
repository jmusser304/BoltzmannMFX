#ifndef BMX_LEVEL_H_
#define BMX_LEVEL_H_

#include <iostream>
#include <memory>
#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <AMReX_AmrCore.H>
#include <AMReX_BLProfiler.H>
#include <AMReX_iMultiFab.H>
#include <AMReX_MLMG.H>
#include <AMReX_PhysBCFunct.H>

#include <bmx_diffusion_op.H>
#include <bmx_pc.H>
#include <bmx_bc_list.H>
#include <bmx_dem_parms.H>
#include <bmx_leveldata.H>

// Deposition scheme enumeration
 enum DepositionScheme {trilinear, square_dpvm, true_dpvm, centroid};

using namespace amrex;

class bmx : public AmrCore
{
  public:
    // Constructor
    bmx ();

    // Destructor
    ~bmx ();

    // Face-based coefficients b in implicit diffusion solve
    Vector< Array<MultiFab*,3> > bcoeff;

    void InitParams ();

    void Init (Real time);

    void InitLevelData (Real time);

    void bmx_init_solvers ();

    void bmx_setup_solvers ();

    void PostInit (Real& dt, Real time, int restart_flag, Real stop_time);

    void ResizeArrays ();

    void InitialRedistribution (amrex::Real l_time);

    void WriteCheckPointFile (std::string & check_file_name,
                              int nstep = 0, Real dt = 0.0, Real time = 0.0);

    void WritePlotFile (std::string & plot_file_name,
                        int nstep = 0, Real time = 0.0);

    void Restart (std::string& restart_chkfile, int* nstep, Real* dt, Real* time);

    void WriteParticleAscii (std::string& par_ascii_file_name, int nstep = 0) const;

    void Regrid ();

    void Evolve (int nstep, Real & dt, Real & prev_dt, Real time, Real stop_time);

    const Geometry get_geom_ref (int lev)
    { return geom[lev]; }

    void InitIOChkData ();

    void ResetIOChkData ();

    void InitIOPltData ();

    // If > 0, dump plotfiles every "plot_int" steps
    static int plot_int;

    // If > 0, dump plotfiles at as close as possible to the designated period
    // *without* changing dt
    static Real plot_per_approx;

    // If > 0, dump plotfiles at exactcly the designated period by changing dt
    static Real plot_per_exact;

    static inline std::string get_load_balance_type ()
    { return load_balance_type; }

    void bmx_calc_txfr_particle (Real time);
    void bmx_calc_txfr_fluid    (Real time);

    void bmx_set_chem_species_bcs (Real time,
                               Vector< MultiFab* > const& X_gk,
                               Vector< MultiFab* > const& D_gk);

    void set_chem_species_diffusivities_g_bcs (Real time,
                                          const int lev,
                                          FArrayBox& D_gk_fab,
                                          const Box& domain);

    void set_chem_species_bc0 (const Box& sbx,
                          MFIter* mfi,
                          const int lev,
                          const Box& domain);

    static DepositionScheme m_deposition_scheme;

    static amrex::Real m_deposition_scale_factor;

    void bmx_calc_volume_fraction (MultiFab& ep_g);

    void bmx_deposition_bcs (int lev, amrex::MultiFab & filled_mf);

    Real volWgtSum (int lev, const MultiFab & mf, int comp, bool local=false) const;

    Real volWgtSumBox (int lev, const MultiFab & mf, int comp, const Box a_bx, bool local=false) const;

    void bmx_set_bc_type (int lev);

    void set_bcrec_lo (const int lev, const int dir, const int l_type);
    void set_bcrec_hi (const int lev, const int dir, const int l_type);

    ////////////////////////////////////////////////////////////////////////////
    //
    // Diffusion
    //
    ////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<DiffusionOp> diffusion_op;

  protected:
    //! Tagging cells for refinement
    virtual void ErrorEst (int lev,
                           TagBoxArray & tags,
                           Real time,
                           int ngrow) override;

    //! Make a new level using provided BoxArray and DistributionMapping and
    //! fill with interpolated coarse level data. Called by AmrCore::regrid.
    virtual void MakeNewLevelFromCoarse (int /*lev*/, Real /*time*/,
                                         const BoxArray& /*ba*/,
                                         const DistributionMapping& /*dm*/) override
    { amrex::Abort("MakeNewLevelFromCoarse: To be implemented"); }

    //! Remake an existing level using provided BoxArray and
    //! DistributionMapping and fill with existing fine and coarse data. Called
    //! by AmrCore::regrid.
    virtual void RemakeLevel (int /*lev*/,
                              Real /*time*/,
                              const BoxArray & /*ba*/,
                              const DistributionMapping & /*dm*/) override
    { amrex::Abort("RemakeLevel: To be implemented"); }

    //! Delete level data.  Called by AmrCore::regrid.
    virtual void ClearLevel (int /*lev*/) override
    { amrex::Abort("ClearLevel: To be implemented"); }

    void bmx_init_fluid (int is_restarting, Real dt, Real stop_time);

    void bmx_set_bc0 ();

    BCList bc_list;

    Vector< Gpu::DeviceVector<Real> > m_bc_X_gk;
    Gpu::DeviceVector<Real*> m_bc_X_gk_ptr;
    Vector<Vector<Real> > m_h_bc_X_gk;

    std::map< std::string, Gpu::DeviceVector<int> > m_X_gk_bc_types;

  protected:
    void FillPatchChemSpecies (int lev, Real time, MultiFab & mf, int icomp, int ncomp,
                           const Vector<BCRec> & bcr);

    void GetDataChemSpecies (int lev, Real time, Vector<MultiFab *> & data, int icomp,
                         Vector<Real> & datatime);

  protected:
    void AllocateArrays (int lev);

    void RegridArrays (int lev);

    void RegridLevelSetArray (int a_lev);

    void avgDown (int crse_lev, const MultiFab & S_fine, MultiFab & S_crse);

  private:

    void MakeBCArrays ();

    void MakeNewLevelFromScratch (int lev, Real time, const BoxArray & new_grids,
                                  const DistributionMapping & new_dmap) override;

    void ReMakeNewLevelFromScratch (int lev, const BoxArray & new_grids,
                                    const DistributionMapping & new_dmap);

    void WriteCheckHeader (const std::string & name, int nstep, Real dt, Real time) const;

    void WriteJobInfo (const std::string & dir ) const;

    static void GotoNextLine (std::istream & is);

    void EvolveFluid (int nstep, Real & dt, Real & time, Real & prev_dt, Real stop_time, Real & drag_timing);

    void check_for_nans (int lev);


    void fillpatch_all (Vector< MultiFab* > const& X_gk_in, Real time);

  public:
    void bmx_apply_predictor ( Vector< MultiFab* >& chem_species_RHS_old,
                               Vector< MultiFab* >& lap_X_old,
                               Vector< MultiFab* >& lap_X_star,
                               Real time,
                               Real dt,
                               Real prev_dt);

    void bmx_chem_species_X_rhs(Vector< MultiFab*      > const& rhs);

  public:

    void bmx_normalize_fluid_chem_species(const Vector< MultiFab* >& X_gk);

  private:

    // Make a level 0 grids covering the whole domain.  It does NOT install the new grids.
    BoxArray MakeBaseGrids () const;

    void ChopGrids (const Box& domain, BoxArray& ba, int target_size) const;

    static int nlev;

    // Particle container
    BMXParticleContainer* pc;

    // Boundary conditions types
    Vector< IArrayBox* > bc_ilo;
    Vector< IArrayBox* > bc_ihi;
    Vector< IArrayBox* > bc_jlo;
    Vector< IArrayBox* > bc_jhi;
    Vector< IArrayBox* > bc_klo;
    Vector< IArrayBox* > bc_khi;

    Vector< std::unique_ptr<LevelData> > m_leveldata;

    Vector< MultiFab* > get_X_gk () noexcept;
    Vector< MultiFab* > get_X_gk_old () noexcept;
    Vector< MultiFab* > get_D_gk () noexcept;

    amrex::Vector< amrex::MultiFab const* > get_X_gk_const () const noexcept;
    amrex::Vector< amrex::MultiFab const* > get_X_gk_old_const () const noexcept;

    amrex::Vector< amrex::MultiFab const* > get_D_gk_const () const noexcept;

    amrex::Vector<int> const& get_chem_species_iconserv () const noexcept { return m_iconserv_chem_species; }
  
    int const* get_chem_species_iconserv_device_ptr () const noexcept {
        return m_iconserv_chem_species_d.data(); }

    amrex::Vector<amrex::BCRec> const& get_chem_species_bcrec () const noexcept { return m_bcrec_chem_species; }
 
    amrex::BCRec const* get_chem_species_bcrec_device_ptr () const noexcept {
      return m_bcrec_chem_species_d.data(); }

    // used if load_balance_type == "KnapSack"
    amrex::Vector< amrex::MultiFab* > particle_cost;
    amrex::Vector< amrex::MultiFab* > fluid_cost;

    // Level-Set Data => used for particle-wall collisions and fluid
    // reconstruction in particle drag calculation. NOTE: this has at least 2
    // levels: 0) fluid reconstruction, 1..N) particle-wall (refined) collisions
    Vector< std::unique_ptr<MultiFab> > level_sets;

    bool advect_fluid_chem_species   = false;

    enum struct DiffusionType {
            Invalid, Explicit, Crank_Nicolson, Implicit
    };
    DiffusionType m_diff_type = DiffusionType::Invalid;

    amrex::Real enthalpy_source = 0;

    // Options to control bmx verbosity level
    int m_verbose = 0;

    // ***************************************************************

    int call_udf;

    bool dual_grid = false;

    // If true then we are restarting from a file with no thermal variables
    bool restart_from_cold_flow = false;

    // If true then print the name of the routine we are in
    bool ooo_debug = false;

    // Parameters to control load balancing
    static std::string particle_init_type;
    static std::string load_balance_type;
    static std::string knapsack_weight_type;
    static int load_balance_fluid;
    static int knapsack_nmax;

    // Options to control time stepping
    Real m_cfl = 0.5;
    Real fixed_dt;
    Real dt_min = 0.;    // Min dt allowed
    Real dt_max = 1.e14; // Max dt allowed

    int particle_max_grid_size_x = -1;
    int particle_max_grid_size_y = -1;
    int particle_max_grid_size_z = -1;

    int removeOutOfRange = 1; // Flag to remove out-of-range particles

    Periodicity p0_periodicity;

    Real sum_vol_orig;

    Real covered_val = 1.e40;

    // Last step at which we wrote a plotfile
    int last_plt = -1;

    // Flags for saving fluid data in plot files
    int plt_X_gk    = 1;
    int plt_D_gk    = 0;

    // Total number of variables to write in plot file
    int pltVarCount = 0;

    //
    // Here we set the number of ghost nodes of the field arrays.
    // Since this is  a class attribute, it will be propagated to
    // all the member functions.
    //
    // DO NOT USE  A LOCAL VARIABLE WITH THE SAME NAME
    //

    const int nghost = 1;

    IntVect particle_sorting_bin = IntVect::TheZeroVector();

     /***************************************************************************
     *                                                                         *
     * I/O stuff                                                               *
     *                                                                         *
     **************************************************************************/

    // Variables to simplify checkpoint IO
    // Vector< Vector< MultiFab* > > vectorVars;
    Vector< std::string > vecVarsName;

    Vector< Vector< MultiFab* > > chkScalarVars;
    Vector< std::string > chkscaVarsName;

    Vector< Vector< MultiFab* > > chkTVars;
    Vector< std::string > chkTVarsName;

    Vector< Vector< MultiFab* > > chkChemSpeciesVars;
    Vector< std::string > chkChemSpeciesVarsName;

    Vector< Vector< MultiFab* > > chkChemSpeciesTVars;
    Vector< std::string > chkChemSpeciesTVarsName;

    /***************************************************************************
     *                                                                         *
     * Used for fillpatching ...                                               *
     *                                                                         *
     **************************************************************************/

    Vector<Real> t_old;
    Vector<Real> t_new;

    Vector<BCRec> bcs_X; // For fluid chem_species
    Vector<BCRec> bcs_f; // For generic first-order extrapolation

    amrex::Vector<amrex::BCRec> m_bcrec_chem_species;

    amrex::Gpu::DeviceVector<amrex::BCRec> m_bcrec_chem_species_d;

    int nghost_state() const { return 1; }

    // Invalid, Explicit, Crank_Nicolson, Implicit
    DiffusionType predictor_diff_type () const{
        return DiffusionType::Implicit;
    }

    amrex::Vector<int> m_iconserv_chem_species;
    amrex::Gpu::DeviceVector<int> m_iconserv_chem_species_d;
};
#endif
