#ifndef IRIS_FIT_H
#define IRIS_FIT_H

#include <dkl.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace iris {

struct fitter {
    virtual bool operator()();

    //interface to be implemented
    virtual int eval(int m, int n, const double *p, double *fvec) const = 0;
    virtual int num_parameter() const = 0;
    virtual int num_variables() const = 0;

    virtual double *params() = 0;

    // optional interface
    virtual double tolerance() const {
        return 1.49012e-8;
    }

protected:
    int fit_info;
};


class gamma_fitter : public fitter {
public:

    gamma_fitter(const std::vector<double> x, const std::vector<double> y) : x(x), y(y) {
        res[0] = y[0];
        res[1] = 0.0003;
        res[2] = 2.2;

    }

    static double func(const double Azero, const double A, const double gamma, const double x) {
        return Azero + A * std::pow(x, gamma);
    }

    virtual int eval(int m, int n, const double *p, double *fvec) const override;

    double Azero() const {
        return res[0];
    }

    double A() const {
        return res[1];
    }

    double gamma() const {
        return res[2];
    }


    virtual int num_parameter() const override {
        return 3;
    }

    virtual int num_variables() const override {
        return static_cast<int>(x.size());
    }

    virtual double *params() override {
        return res;
    }

private:
    std::vector<double> x;
    std::vector<double> y;
    double res[3];
};

class sin_fitter : public fitter {
public:
    sin_fitter(const std::vector<double> &x, const std::vector<double> &y, bool fit_freq = false, double offset = -1)
            : x(x), y(y), fit_frequency(fit_freq) {

        auto result = std::minmax_element(y.cbegin(), y.cend());
        size_t p_min = std::distance(y.begin(), result.first);
        size_t p_max = std::distance(y.begin(), result.second);

        double v_min = y[p_min];
        double v_max = y[p_max];

        double v_amp = (v_max - v_min) * 0.5;
        double v_mid = v_min + (v_max - v_min) * 0.5;

        if ((fit_offset = offset < 0)) {
            dc = v_mid;
            freq_idx = 3;
        } else {
            dc = offset;
            freq_idx = 2;
        }

        p[0] = v_amp;    // amplitude
        p[1] = x[p_max]; // phase
        p[2] = dc;       // offset
        p[freq_idx] = 1; // frequency
    }

    virtual int eval(int m, int n, const double *p, double *fvec) const override;

    virtual int num_parameter() const override {
        int params = 4;
        params -= fit_frequency ? 0 : 1;
        params -= fit_offset ? 0 : 1;
        return params;
    }

    virtual int num_variables() const override {
        return static_cast<int>(x.size());
    }

    virtual double *params() override {
        return p;
    }

    double amplitude() const { return p[0]; }
    double phase() const { return p[1]; }
    double offset() const { return fit_offset ? p[2] : dc; }
    double frequency() const { return fit_frequency ? p[freq_idx] : 1.0; }

    const std::vector<double> &x;
    const std::vector<double> &y;
    bool fit_frequency;
    bool fit_offset;
    double dc = 0.66;
    double p[4];
    size_t freq_idx;
};

class rgb2sml_fitter : public fitter {
public:
    rgb2sml_fitter(const std::vector<double> &x, const std::vector<double> &y, const double weight_exponent = 1.1)
            : x(x), y(y), we(weight_exponent) {
        res[0] = res[1] = res[2] = 0.01;

        res[3] = res[6] = res[9] = 0.00005;
        res[4] = res[7] = res[10] = 0.00001;
        res[5] = res[8] = res[11] = 0.00001;

        res[12] = res[13] = res[14] = 0.9;
    }

    virtual int eval(int m, int n, const double *p, double *fvec) const override;

    virtual int num_parameter() const override {
        return 15; // 3 * Ao (AoS, AoM, AoL), 3 * gamma (R, G, B), 3x3 A, Matrix(ArS, AgS..)
    }

    virtual int num_variables() const override {
        return static_cast<int>(x.size());
    }

    virtual double *params() override {
        return res;
    }

    dkl::parameter rgb2sml() const {
        dkl::parameter pr;
        memcpy(&pr, res, sizeof(res));
        return pr;
    }

    const std::vector<double> &x;
    const std::vector<double> &y;
    const double we;
    double res[15];
};

}

#endif