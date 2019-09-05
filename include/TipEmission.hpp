#ifndef _TIP_EMISSION_
#define _TIP_EMISSION_

#include <string>
#include <vector>
#include <array>

#include "SpheroidScattering.hpp"
#include "FowlerNordheimEmission.hpp"

class TipEmission {
public:
    TipEmission(std::string folder) {
        double tipRadius= std::stod(folder.substr(folder.find("R=") + 2, folder.find("L=_") - (folder.find("R=") + 2))) * 1.0e-9;
        double length = std::stod(folder.substr(folder.find("_L=") + 3, folder.find("f0=_") - (folder.find("_L=") + 3))) * 1.0e-6;
        spheroid = SpheroidScattering(tipRadius, length);
        spheroid.SetTemporalFieldInterpolators(folder);
    }

    void SetElectricFieldAmplitude(double e_max) {
        eField_si = e_max;
    }

    void SetMetalWorkFunction(double u_ev) {
        workFunction_eV = u_ev;
    }

    auto& GetSpheroid() {
        return spheroid;
    }

    std::vector<double>& GetTimeSamples() {
        return spheroid.GetTemporalSamples();
    }

    double GetEtaMin(double distanceToTop) {
        double eta_min, _ksi_, _phi_;
        spheroid.CoordinatePointTransformRectToSpheroid(0.0, 0.0, spheroid.GetLength()/2.0 - distanceToTop,
                                                        eta_min, _ksi_, _phi_);
        return eta_min;
    }

    void SubdevideSurface(double distanceToTop, double max_patch_area) {
        auto& t_arr = GetTimeSamples();

        int n_t = t_arr.size();

        double eta_min = GetEtaMin(distanceToTop);
        std::cout << "eta_min : " << eta_min << std::endl;


        double max_surface_area = max_patch_area;
        spheroid.SubdevideSurface_Cartesian(eta_min, max_surface_area, r_pts_cart, r_pts_sph,
                                                     normal_vec_cart, normal_vec_sph,
                                                     surfaceArea);

        std::size_t n_patch = r_pts_cart.size();
        assert(r_pts_sph.size() == n_patch && normal_vec_cart.size() == n_patch
               && normal_vec_sph.size() == n_patch && surfaceArea.size() == n_patch);
        eFieldNormal.resize(n_patch, 0.0);
    }

    auto GetTotalNumberOfEmittedParticles() {
        auto& t_arr = GetTimeSamples();
        int n_t = t_arr.size();

        std::size_t n_patch = r_pts_cart.size();

        std::vector<double> n_particles(n_patch, 0.0);
        std::vector<std::complex<double>> e_eta, e_ksi, e_phi;

        for(int i = 1; i < n_t; ++i) {
            spheroid.GetTemporalFieldAtGridPoints_SpatialInterpolation(r_pts_sph, e_eta, e_ksi, e_phi, i);

            for(int j = 0; j < n_patch; ++j) {
                double e_normal = eField_si*std::real(e_ksi[j]);

                if (e_normal < 0.0) {
                    FowlerNordheimEmission fn(std::abs(e_normal), workFunction_eV);

                    double n_elec = fn.GetNumberOfEmittedElectrons(surfaceArea[j], t_arr[i] - t_arr[i - 1]);
                    n_particles[j] += n_elec;
                }
            }
        }

        return n_particles;
    }

    auto GetNumberOfEmittedParticles(int timeInd) {
        auto& t_arr = GetTimeSamples();
        int n_t = t_arr.size();

        std::size_t n_patch = r_pts_cart.size();

        std::vector<double> n_particles(n_patch, 0.0);
        std::vector<std::complex<double>> e_eta, e_ksi, e_phi;

        if (timeInd == 0) {
            for(int j = 0; j < n_patch; ++j) {
                eFieldNormal[j] = 0.0;
            }
            return n_particles;
        }

        assert(timeInd >= 1);
        int i = timeInd;

        spheroid.GetTemporalFieldAtGridPoints_SpatialInterpolation(r_pts_sph, e_eta, e_ksi, e_phi, i);
        for(int j = 0; j < n_patch; ++j) {
            double e_normal = eField_si*std::real(e_ksi[j]);
            eFieldNormal[j] = e_normal;

            if (e_normal < 0.0) {
                FowlerNordheimEmission fn(std::abs(e_normal), workFunction_eV);

                double n_elec = fn.GetNumberOfEmittedElectrons(surfaceArea[j], t_arr[i] - t_arr[i - 1]);
                n_particles[j] += n_elec;
            }
        }


        return n_particles;
    }

    auto& GetEmissionPoints() {
        return r_pts_cart;
    }

    auto& GetEmissionPointNormals() {
        return normal_vec_cart;
    }

    auto& GetNormalEFields() {
        return eFieldNormal;
    }

    void GetElectricForce(std::vector<double>& charges, std::vector<std::array<double, 3>>& positions,
                          std::vector<std::array<double, 3>>& forces, int timeInd) {
        std::size_t n_pts = charges.size();
        assert(positions.size() == n_pts);
        std::vector<std::array<double, 3>> positions_sph(n_pts);

        for(std::size_t i = 0; i < n_pts; ++i) {
            auto& r_i = positions[i];
            auto& r_i_sph = positions_sph[i];
            spheroid.CoordinatePointTransformRectToSpheroid(r_i[0], r_i[1], r_i[2], r_i_sph[0], r_i_sph[1], r_i_sph[2]);
        }

        std::vector<std::complex<double>> e_eta, e_ksi, e_phi;
        spheroid.GetTemporalFieldAtGridPoints_SpatialInterpolation(positions_sph, e_eta, e_ksi, e_phi, timeInd);

        std::vector<std::complex<double>> e_x_pts;
        std::vector<std::complex<double>> e_y_pts;
        std::vector<std::complex<double>> e_z_pts;
        spheroid.VectorTransformFromSpheroidToRect(positions_sph, e_eta, e_ksi, e_phi,
                                                                  e_x_pts, e_y_pts, e_z_pts);

       forces.resize(n_pts);
       double e_charge = -PhysicalConstants_SI::electronCharge;
       for(std::size_t i = 0; i < n_pts; ++i) {
            forces[i] = std::array<double, 3>{e_charge * std::real(e_x_pts[i]),
                                              e_charge * std::real(e_y_pts[i]),
                                              e_charge * std::real(e_z_pts[i])};
       }
    }

private:
    SpheroidScattering spheroid;

    std::vector<std::array<double, 3>> r_pts_cart;
    std::vector<std::array<double, 3>> r_pts_sph;
    std::vector<std::array<double, 3>> normal_vec_cart;
    std::vector<std::array<double, 3>> normal_vec_sph;
    std::vector<double> surfaceArea;
    std::vector<double> eFieldNormal;

    double eField_si;
    double workFunction_eV;
};


#endif // _TIP_EMISSION_
