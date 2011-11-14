#ifndef FAUNUS_MCMOVE_H
#define FAUNUS_MCMOVE_H

#include <faunus/common.h>
#include <faunus/point.h>
#include <faunus/average.h>
#include <faunus/energy.h>

namespace Faunus {
  //class average;
  class unittest;
  class Energybase;

  /*!
   * \brief Monte Carlo move related classes
   */
  namespace Move {

    /*!
     * \brief Optimize Monte Carlo displacement parameters for optimal mean square displament
     * \date Lund, 2011
     * \author Mikael Lund
     */
    class DisplacementOptimizer {
      private:
        double* dpPtr;   //!< Pointer to displacement parameter
        //average<double>* msqPtr; //!< Pointer to mean-square displacement
        double dp_min;
        double dp_max;

      public:
        //void set_target(double&, average&);
    };

    /*!
     * \brief Base class for Monte Carlo moves
     * \author Mikael Lund
     * \date Lund, 2007-2011
     *
     * The is a base class and derived classes MUST implement a number of
     * functions. Importanty, it is important to have an argumentfree move()
     * function as this can be attached to groups so that these have the
     * information to move automatically.
     */

    class Movebase {
      private:
        unsigned long int cnt_accepted;        //!< number of accepted moves
        double dusum;                          //!< Sum of all energy changes made by this move
                                              
        virtual void _test(UnitTest&);         //!< Unit testing
        virtual string _info()=0;              //!< Specific info for derived moves
        virtual void _trialMove()=0;           //!< Do a trial move
        virtual void _acceptMove()=0;          //!< Accept move, store new coordinates.
        virtual void _rejectMove()=0;          //!< Reject move, revert to old coordinates.
        virtual double _energyChange()=0;      //!< Returns energy change of trialMove
        virtual void trialMove() final;        //!< Do a trial move (wrapper)
        virtual void acceptMove() final;       //!< Accept move, store new coordinates etc. (wrapper)
        virtual void rejectMove() final;       //!< Reject move, revert to old coordinates etc. (wrapper)
        virtual double energyChange() final;   //!< Returns energy change of trialMove (wrapper)
        bool metropolis(const double&) const;  //!< Metropolis criteria

      protected:
        Energy::Energybase* pot;         //!< Pointer to energy functions
        Space* spc;
        string title;                    //!< title of move
        string cite;                     //!< litterature reference, url, DOI etc.
        string prefix;                   //!< inputmap prefix
        char w;                          //!< info string text width
        unsigned long int cnt;           //!< total number of trial moves
        bool run() const;                //!< Runfraction test

      public:
        Movebase(Energy::Energybase&, Space&, string);             //!< Constructor
        virtual ~Movebase();
        double move(int=1);          //!< Attempt \c n moves and return energy change
        double runfraction;          //!< Fraction of times calling move() should result in an actual move
        string info();               //!< Returns information string
        void test(UnitTest&);        //!< Perform unit test
    };

    /*!
     * \brief Translation of atomic particles
     * \author Mikael Lund
     * \date Lund, 2011
     *
     * This Markov move can work in two modes:
     * \li Move a single particle
     * \li Move single particles randomly selected in a Group.
     *
     * To move a single particle, specify its position in the space particle vector
     * in iparticle. For randomly moving all particles in a group (typically salt),
     * point igroup to the appropriate Group in the space class g vector.
     */
    class ParticleTranslation : public Movebase {
      private:
        typedef std::map<short, Average<double> > map_type;
        map_type accmap; //!< Single particle acceptance map
        map_type sqrmap; //!< Single particle mean square displacement map
        Group* igroup;   //!< Group pointer in which particles are moved randomly (NULL if none, default)
        int iparticle;   //!< Select single particle to move (-1 if none, default)
        Average<unsigned int> gsize; //!< Average size of igroup;

        string _info();
        void _trialMove();
        void _acceptMove();
        void _rejectMove();
        double _energyChange();
      public:
        ParticleTranslation(InputMap&, Energy::Energybase&, Space&, string="mv_particle");
        void setGroup(Group&); //!< Select group in which to randomly pick particles from
        void setParticle(int); //!< Select single particle in p_vec to move
        Point dir;             //!< Displacement directions (default: x=y=z=1)
    };

    /*!
     * \brief Combined rotation and rotation of groups
     * \author Mikael Lund
     */
    class RotateGroup : public Movebase {
      private:
        void _test(UnitTest&);
        void _trialMove();
        void _acceptMove();
        void _rejectMove();
        double _energyChange();
        string _info();
        typedef std::map<string, Average<double> > map_type;
        map_type accmap;   //!< Group particle acceptance map
        map_type sqrmap_t; //!< Group mean square displacement map (translation)
        map_type sqrmap_r; //!< Group mean square displacement map (rotation)
        Group* igroup;
        double dp_rot;     //!< Rotational displament parameter
        double dp_trans;   //!< Translational displacement parameter
        double angle;      //!< Temporary storage for current angle
      public:
        RotateGroup(InputMap&, Energy::Energybase&, Space&, string="transrot");
        void setGroup(Group&); //!< Select Group to move
        Point dir;             //!< Translation directions (default: x=y=z=1)
        bool groupWiseEnergy;  //!< Attempt to evaluate energy over groups from vector in Space (default=false)
    };

    /*!
     * \brief Isobaric volume move
     *
     * This class will perform a volume displacement and scale atomic as well as molecular
     * groups as long as these are known to Space -- see Space.enroll().
     * The constructor will automatically add an instance of Energy::ExternalPressure
     * to the Hamiltonian. The InputMap class is scanned for the following keys:
     * \li \c npt_dV \n Volume displacement parameter
     * \li \c npt_P \n Pressure
     * \li \c npt_Punit \n Pressure unit: mM [default] or 1/A3
     *
     * Note that the volume displacement is done by:
     *
     * \f$ V^{\prime} = \exp\left ( \log V \pm \delta dV \right ) \f$ where \f$\delta\f$ is a random number
     * between zero and one half.
     *
     * Example:
     * \code
     * Energy::Hamiltonian pot;        // we need a hamiltonian
     * ...                             // insert interactions...
     * Space spc( pot.getGeometry() ); // ...and a space
     * spc.enroll( myprotein );        // register some groups
     * spc.enroll( mysolvent );
     * Move::Isobaric(in, pot, spc);   // setup and add pressure to hamiltonian
     * \endcode
     */
    class Isobaric : public Movebase {
      private:
        Energy::Hamiltonian* hamiltonian;
        string _info();
        void _test(UnitTest&);
        void _trialMove();
        void _acceptMove();
        void _rejectMove();
        double _energy(const p_vec&);
        double _energyChange();
        double dV; //!< Volume displacement parameter
        double oldV;
        double newV;
        double P; //!< Pressure
        Average<double> sqrV;       //!< Mean squared volume displacement
        Average<double> V;          //!< Average volume
      public:
        Isobaric(InputMap&, Energy::Hamiltonian&, Space&, string="npt");
    };

    /*!
     * \brief Auxillary class for tracking atomic species
     * \author Mikael Lund
     * \date Malmo 2011
     *
     * This class keeps track of individual particle positions based on particle type (id).
     * It contains functions to insert and erase particles while automatically moving particles
     * above the deletion or insertion point in sync.
     *
     * Example:
     * \code
     * AtomTracker track(myspace);
     * track.insert( myparticle );
     * ...
     * int i=track[ myparticle.id ].random();
     * \endcode
     */
    class AtomTracker {
      private:
        typedef short Tid;  // particle id type
        typedef int Tindex; // particle index type
        Space* spc;
        class data {
          public:
            vector<Tindex> index;
            Tindex random();                  //!< Pick random particle index
        };
        std::map<Tid,data> map; 
      public:
        AtomTracker(Space&);
        Tid randomAtomType() const;           //!< Select a random atomtype from the list
        bool insert(const particle&, Tindex); //!< Insert particle into END OF(!) Space and track position
        bool erase(Tindex);                   //!< Delete particle from Space at specific particle index
        data& operator[] (Tid);               //!< Access operator to atomtype data
        void clear();
        bool empty();
    };

    /*!
     * \brief Grand Canonical insertion of arbitrary M:X salt pairs
     * \author Bjorn Persson and Mikael Lund
     * \date Lund 2010-2011
     * \todo GC salt must be at end of particle. Remedy this.
     */
    class gcbath : public Movebase {
      private:
        string _info();
        void _trialMove();
        void _acceptMove();
        void _rejectMove();
        double _energyChange();
        void add(Group&);       // add salt group and scan for ions with non-zero activities

        AtomTracker tracker;
        struct ionprop {
          particle p;
          double chempot;
          Average<double> rho;
        };
        std::map<short,ionprop> map;
        void randomIonPair(short&,short&);
        p_vec trial_insert;
        vector<int> trial_delete;
        short ida, idb;

        Energy::EnergyRest Urest;
        double du_rest;
        Group* saltPtr;  //!< GC ions *must* be in this group

      public:
        gcbath(InputMap&, Energy::Hamiltonian&, Space&, Group&, string="saltbath");
    };


  }//namespace
}//namespace
#endif
