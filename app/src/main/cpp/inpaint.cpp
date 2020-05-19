#include <algorithm>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <android/log.h>
#include "inpaint.h"

namespace {
    static std::vector<double> kDistance2Similarity;

    void init_kDistance2Similarity() {
        double base[11] = {1.0, 0.99, 0.96, 0.83, 0.38, 0.11, 0.02, 0.005, 0.0006, 0.0001, 0};
        int length = (MaskedImage::kDistanceScale + 1);
        kDistance2Similarity.resize(length);
        for (int i = 0; i < length; ++i) {
            double t = (double) i / length;
            int j = (int) (100 * t);
            int k = j + 1;
            double vj = (j < 11) ? base[j] : 0;
            double vk = (k < 11) ? base[k] : 0;
            kDistance2Similarity[i] = vj + (100 * t - j) * (vk - vj);
        }
    }
}

/**
 * This algorithme uses a version proposed by Xavier Philippeau.
 */

Inpainting::Inpainting(cv::Mat image, cv::Mat mask, int patch_size)
    : m_initial(image, mask), m_patch_size(patch_size), m_pyramid(), m_source2target(), m_target2source() {

    auto source = m_initial;
    m_pyramid.push_back(source);
    while (source.size().height > m_patch_size && source.size().width > m_patch_size) {
        source = source.downsample();
        m_pyramid.push_back(source);
    }

    if (kDistance2Similarity.size() == 0) {
        init_kDistance2Similarity();
    }
}

cv::Mat Inpainting::run(bool verbose) {
    const int nr_levels = m_pyramid.size();

    MaskedImage source, target;
    for (int level = nr_levels-1; level >= 1; --level) {
        if (verbose) {
            char str[50];
            sprintf(str,"Inpainting level: %d",level);
            __android_log_print(ANDROID_LOG_INFO,"Inpainting Now",str,__FUNCTION__);
        }

        source = m_pyramid[level];

        if (level == nr_levels - 1) {
            target = source.clone();
            target.clear_mask();

            m_source2target = NearestNeighborField(source, target, m_patch_size);
            m_target2source = NearestNeighborField(target, source, m_patch_size);
        } else {
            m_source2target = NearestNeighborField(source, target, m_patch_size, m_source2target);
            m_target2source = NearestNeighborField(target, source, m_patch_size, m_target2source);
        }

        if (verbose) __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","initialize done",__FUNCTION__);


        target = _expectation_maximization(source, target, level, verbose);
    }
    __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","Done",__FUNCTION__);
    return target.image();
}

// EM-Like algorithm (see "PatchMatch" - page 6).
// Returns a double sized target image (unless level = 0).
MaskedImage Inpainting::_expectation_maximization(MaskedImage source, MaskedImage target, int level, bool verbose) {
    const int nr_iters_em = 1 + 2 * level;
    const int nr_iters_nnf = static_cast<int>(std::min(7, 1 + level));

    MaskedImage new_source, new_target;

    for (int iter_em = 0; iter_em < nr_iters_em; ++iter_em) {
        if (iter_em != 0) {
            m_source2target.set_target(new_target);
            m_target2source.set_source(new_target);
            target = new_target;
        }

        char str[50];
        sprintf(str,"EM Iteration: %d",iter_em);
        __android_log_print(ANDROID_LOG_INFO,"Inpainting Now",str,__FUNCTION__);
        auto size = source.size();
        for (int i = 0; i < size.height; ++i) {
            for (int j = 0; j < size.width; ++j) {
                if (!source.contains_mask(i, j, m_patch_size)) {
                    m_source2target.set_identity(i, j);
                    m_target2source.set_identity(i, j);
                }
            }
        }
        m_source2target.minimize(nr_iters_nnf);
        m_target2source.minimize(nr_iters_nnf);
        __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","  NNF minimization finished.",__FUNCTION__);

        // Instead of upsizing the final target, we build the last target from the next level source image.
        // Thus, the final target is less blurry (see "Space-Time Video Completion" - page 5).
        bool upscaled = false;
        if (level >= 1 && iter_em == nr_iters_em - 1) {
            new_source = m_pyramid[level - 1];
            new_target = target.upsample(new_source.size().width, new_source.size().height);
            upscaled = true;
        } else {
            new_source = m_pyramid[level];
            new_target = target.clone();
        }


        auto vote = cv::Mat(new_target.size(), CV_64FC4);
        vote.setTo(cv::Scalar::all(0));

        // Votes for best patch from NNF Source->Target (completeness) and Target->Source (coherence).
        _expectation_step(m_source2target, 1, vote, new_source, upscaled);
        __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","  Expectation source to target finished.",__FUNCTION__);
        _expectation_step(m_target2source, 0, vote, new_source, upscaled);
        __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","  Expectation target to source finished.",__FUNCTION__);

        // Compile votes and update pixel values.
        _maximization_step(new_target, vote);
        __android_log_print(ANDROID_LOG_INFO,"Inpainting Now","  Minimization step finished.",__FUNCTION__);
    }

    return new_target;
}

// Expectation step: vote for best estimations of each pixel.
void Inpainting::_expectation_step(
    const NearestNeighborField &nnf, bool source2target,
    cv::Mat &vote, const MaskedImage &source, bool upscaled
) {
    auto source_size = nnf.source_size();
    auto target_size = nnf.target_size();

    for (int i = 0; i < source_size.height; ++i) {
        for (int j = 0; j < source_size.width; ++j) {
            int yp = nnf.at(i, j, 0), xp = nnf.at(i, j, 1), dp = nnf.at(i, j, 2);
            double w = kDistance2Similarity[dp];

            for (int di = -m_patch_size; di <= m_patch_size; ++di) {
                for (int dj = -m_patch_size; dj <= m_patch_size; ++dj) {
                    int ys = i + di, xs = j + dj, yt = yp + di, xt = xp + dj;
                    if (!(ys >= 0 && ys < source_size.height && xs >= 0 && xs < source_size.width)) continue;
                    if (!(yt >= 0 && yt < target_size.height && xt >= 0 && xt < target_size.width)) continue;

                    if (!source2target) {
                        std::swap(ys, yt);
                        std::swap(xs, xt);
                    }

                    if (upscaled) {
                        for (int uy = 0; uy < 2; ++uy) {
                            for (int ux = 0; ux < 2; ++ux) {
                                _weighted_copy(source, 2 * ys + uy, 2 * xs + ux, vote, 2 * yt + uy, 2 * xt + ux, w);
                            }
                        }
                    } else {
                        _weighted_copy(source, ys, xs, vote, yt, xt, w);
                    }
                }
            }
        }
    }
}

// Maximization Step: maximum likelihood of target pixel.
void Inpainting::_maximization_step(MaskedImage &target, const cv::Mat &vote) {
    auto target_size = target.size();
    for (int i = 0; i < target_size.height; ++i) {
        for (int j = 0; j < target_size.width; ++j) {
            const double *source_ptr = vote.ptr<double>(i, j);
            unsigned char *target_ptr = target.get_mutable_image(i, j);
            if (source_ptr[3] > 0) {
                unsigned char r = cv::saturate_cast<unsigned char>(source_ptr[0] / source_ptr[3]);
                unsigned char g = cv::saturate_cast<unsigned char>(source_ptr[1] / source_ptr[3]);
                unsigned char b = cv::saturate_cast<unsigned char>(source_ptr[2] / source_ptr[3]);
                target_ptr[0] = r, target_ptr[1] = g, target_ptr[2] = b;
            } else {
                target.set_mask(i, j, 0);
            }
        }
    }
}

void Inpainting::_weighted_copy(const MaskedImage &source, int ys, int xs, cv::Mat &target, int yt, int xt, double weight) {
    if (source.is_masked(ys, xs)) return;

    auto source_ptr = source.get_image(ys, xs);
    auto target_ptr = target.ptr<double>(yt, xt);

    for (int c = 0; c < 3; ++c)
        target_ptr[c] += static_cast<double>(source_ptr[c]) * weight;
    target_ptr[3] += weight;
}
