#pragma once

#include "core.h"
#include "group.h"
#include "space.h"
#include <Eigen/Dense>

namespace Faunus {

    namespace ReactionCoordinate {

        /**
         * @brief Base class for reaction coordinates
         */
        struct ReactionCoordinateBase {
            std::function<double()> f=nullptr; // returns reaction coordinate
            virtual void _to_json(json &j) const;
            virtual double normalize(double) const;
            double binwidth=0, min=0, max=0;
            std::string name;

            double operator()(); //!< Calculates reaction coordinate

            bool inRange(double coord) const; //!< Determines if coordinate is within [min,max]
        };

        void to_json(json &j, const ReactionCoordinateBase &r);
        void from_json(const json &j, ReactionCoordinateBase &r);

#ifdef DOCTEST_LIBRARY_INCLUDED
        TEST_CASE("[Faunus] ReactionCoordinateBase")
        {
            using doctest::Approx;
            ReactionCoordinateBase c = R"({"range":[-1.5, 2.1], "resolution":0.2})"_json;
            CHECK( c.min == Approx(-1.5) );
            CHECK( c.max == Approx( 2.1) );
            CHECK( c.binwidth == Approx(0.2) );
            CHECK( c.inRange(-1.5)  == true );
            CHECK( c.inRange(-1.51) == false );
            CHECK( c.inRange( 2.11) == false );
            CHECK( c.inRange( 2.1)  == true );
        }
#endif

        class SystemProperty : public ReactionCoordinateBase {
            protected:
                std::string property;
            public:
                template<class Tspace>
                    SystemProperty(const json &j, Tspace &spc) {
                        name = "system";
                        from_json(j, *this);
                        property = j.at("property").get<std::string>();
                        if (property=="V") f = [&g=spc.geo]() { return g.getVolume();};
                        else if (property=="Lx") f = [&g=spc.geo]() { return g.getLength().x(); };
                        else if (property=="Ly") f = [&g=spc.geo]() { return g.getLength().y(); };
                        else if (property=="Lz" or property=="height") f = [&g=spc.geo]() { return g.getLength().z(); };
                        else if (property=="radius") {
                            if (spc.geo.type==Geometry::Chameleon::SPHERE or spc.geo.type==Geometry::Chameleon::CYLINDER)
                                f = [&g=spc.geo]() { return 0.5*g.getLength().x(); };
                            else
                                std::cerr << "`radius` coordinate unavailable for geometry" << endl;
                        }
                        else if (property=="Q") // system net charge
                            f = [&groups=spc.groups]() {
                                double charge_sum=0;
                                for (auto &g : groups) // loops over groups
                                    for (auto &p : g)  // loop over particles
                                        charge_sum += p.charge;
                                return charge_sum;
                            };

                        if (f==nullptr)
                            throw std::runtime_error(name + ": unknown property '" + property + "'");
                    }
                void _to_json(json &j) const override;
        };

        class AtomProperty : public ReactionCoordinateBase {
            protected:
                size_t index; // atom index
                Point dir={0,0,0};
            public:
                std::string property;
                inline AtomProperty() {}
                template<class Tspace>
                    AtomProperty(const json &j, Tspace &spc) {
                        name = "atom";
                        from_json(j, *this);
                        index = j.at("index");
                        property = j.at("property").get<std::string>();
                        if (property=="x") f = [&i=spc.p.at(index).pos]() { return i.x(); };
                        if (property=="y") f = [&i=spc.p.at(index).pos]() { return i.y(); };
                        if (property=="z") f = [&i=spc.p.at(index).pos]() { return i.z(); };
                        if (property=="q") f = [&i=spc.p.at(index)]()     { return i.charge; };
                        if (property=="R") f = [&i=spc.p.at(index).pos]() { return i.norm(); };
                        if (f==nullptr)
                            throw std::runtime_error(name + ": unknown property '" + property + "'");
                    }
                void _to_json(json &j) const override;
        };

        struct MoleculeProperty : public AtomProperty {
            template<class Tspace>
                MoleculeProperty(const json &j, Tspace &spc) {
                    name = "molecule";
                    from_json(j, *this);
                    index = j.at("index");
                    auto b = spc.geo.getBoundaryFunc();
                    property = j.at("property").get<std::string>();

                    if (property=="confid")     f = [&i=spc.groups.at(index)]() { return i.confid; };
                    else if (property=="com_x") f = [&i=spc.groups.at(index)]() { return i.cm.x(); };
                    else if (property=="com_y") f = [&i=spc.groups.at(index)]() { return i.cm.y(); };
                    else if (property=="com_z") f = [&i=spc.groups.at(index)]() { return i.cm.z(); };
                    else if (property=="N")     f = [&i=spc.groups.at(index)]() { return i.size(); };
                    else if (property=="Q")     f = [&i=spc.groups.at(index)]() { return Geometry::monopoleMoment(i.begin(), i.end()); };

                    else if (property=="mu_x")  f = [&i=spc.groups.at(index), b]() {
                        return Geometry::dipoleMoment(i.begin(), i.end(), b).x(); };
                    else if (property=="mu_y")  f = [&i=spc.groups.at(index), b]() {
                        return Geometry::dipoleMoment(i.begin(), i.end(), b).y(); };
                    else if (property=="mu_z")  f = [&i=spc.groups.at(index), b]() {
                        return Geometry::dipoleMoment(i.begin(), i.end(), b).z(); };
                    else if (property=="mu")    f = [&i=spc.groups.at(index), b]() {
                        return Geometry::dipoleMoment(i.begin(), i.end(), b).norm(); };

                    else if (property=="muangle") {
                        dir = j.at("dir").get<Point>().normalized();
                        if (not spc.groups.at(index).atomic)
                            f = [&i=spc.groups.at(index), b, &dir=dir]() {
                                Point mu = Geometry::dipoleMoment(i.begin(), i.end(), b);
                                return std::acos(mu.dot(dir)) * 180 / pc::pi;
                            };
                    }

                    else if (property=="angle") {
                        dir = j.at("dir").get<Point>().normalized();
                        if (not spc.groups.at(index).atomic) {
                            f = [&spc, &dir=dir, i=index]() {
                                auto &cm = spc.groups[i].cm;
                                auto S = Geometry::gyration(spc.groups[i].begin(), spc.groups[i].end(), spc.geo.getBoundaryFunc(), cm);
                                Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> esf(S);
                                Point eivals = esf.eigenvalues();
                                std::ptrdiff_t i_eival;
                                double minOfeivals = eivals.minCoeff(&i_eival);
                                Point vec = esf.eigenvectors().col(i_eival).real();
                                double cosine = vec.dot(dir);
                                double angle = std::acos(std::fabs(cosine)) * 180. / pc::pi;
                                return angle; 
                            };
                        }
                    }

                    if (f==nullptr)
                        throw std::runtime_error(name + ": unknown or impossible property '" + property + "'");
                }
        };

        /**
         * @brief Reaction coordinate: molecule-molecule mass-center separation
         */
        struct MassCenterSeparation : public ReactionCoordinateBase {
            Eigen::Vector3i dir={1,1,1};
            std::vector<size_t> index;
            std::vector<std::string> type;
            template<class Tspace>
                MassCenterSeparation(const json &j, Tspace &spc) {
                    typedef typename Tspace::Tparticle Tparticle;
                    name = "cmcm";
                    from_json(j, *this);
                    dir = j.value("dir", dir);
                    index = j.at("index").get<decltype(index)>();
                    type = j.at("type").get<decltype(type)>();
                    if (index.size()==2) {
                        f = [&spc, dir=dir, i=index[0], j=index[1]]() {
                            auto &cm1 = spc.groups[i].cm;
                            auto &cm2 = spc.groups[j].cm;
                            return spc.geo.vdist(cm1, cm2).cwiseProduct(dir.cast<double>()).norm(); 
                        };
                    }
                    else if (type.size()==2) {
                        f = [&spc, dir=dir, type1=type[0], type2=type[1]]() {
                            Group<Tparticle> g(spc.p.begin(), spc.p.end());
                            auto slice1 = g.find_id(findName(atoms, type1)->id());
                            auto slice2 = g.find_id(findName(atoms, type2)->id());
                            auto cm1 = Geometry::massCenter(slice1.begin(), slice1.end(), spc.geo.getBoundaryFunc());
                            auto cm2 = Geometry::massCenter(slice2.begin(), slice2.end(), spc.geo.getBoundaryFunc());
                            return spc.geo.vdist(cm1, cm2).cwiseProduct(dir.cast<double>()).sum();
                        };
                    }
                    else
                        throw std::runtime_error(name + ": specify exactly two molecule indeces or two atom types");
                }

            double normalize(double coord) const override; // normalize by volume element

            void _to_json(json &j) const override;
        };

#ifdef DOCTEST_LIBRARY_INCLUDED
        TEST_CASE("[Faunus] MassCenterSeparation")
        {
            using doctest::Approx;
            typedef Space<Geometry::Chameleon, Particle<>> Tspace;
            Tspace spc;
            MassCenterSeparation c( R"({"dir":[1,1,0], "index":[7,8], "type":[] })"_json, spc);
            CHECK( c.dir.x() == 1 );
            CHECK( c.dir.y() == 1 );
            CHECK( c.dir.z() == 0 );
            CHECK( c.index == decltype(c.index)({7,8}) );
        }
#endif

    } // namespace
} // namespace
