#pragma once

#include <opencv2/core.hpp>

class MaskedImage {
public:
    MaskedImage() : m_image(), m_mask() {
        // pass
    }
    MaskedImage(cv::Mat image, cv::Mat mask) : m_image(image), m_mask(mask)  {
        // pass
    }
    MaskedImage(int width, int height) {
        m_image = cv::Mat(cv::Size(width, height), CV_8UC3);
        m_mask = cv::Mat(cv::Size(width, height), CV_8U);
        m_image = cv::Scalar::all(0);
        m_mask = cv::Scalar::all(0);
    }
    inline MaskedImage clone() {
        return MaskedImage(m_image.clone(), m_mask.clone());
    }

    inline cv::Size size() const {
        return m_image.size();
    }
    inline const cv::Mat &image() const {
        return m_image;
    }

    inline bool is_masked(int y, int x) const {
        return static_cast<bool>(m_mask.at<unsigned char>(y, x));
    }
    inline void set_mask(int y, int x, bool value) {
        m_mask.at<unsigned char>(y, x) = static_cast<unsigned char>(value);
    }
    inline void clear_mask() {
        m_mask.setTo(cv::Scalar(0));
    }

    inline const unsigned char *get_image(int y, int x) const {
        return m_image.ptr<unsigned char>(y, x);
    }
    inline unsigned char *get_mutable_image(int y, int x) {
        return m_image.ptr<unsigned char>(y, x);
    }

    inline unsigned char get_image(int y, int x, int c) const {
        return m_image.ptr<unsigned char>(y, x)[c];
    }
    inline int get_image_int(int y, int x, int c) const {
        return static_cast<int>(m_image.ptr<unsigned char>(y, x)[c]);
    }

    bool contains_mask(int y, int x, int patch_size) const;
    MaskedImage downsample() const;
    MaskedImage upsample(int new_w, int new_h) const;

    static const int kDistanceScale;
    static const int kSSDScale;
    static const cv::Size kDownsampleKernelSize;
    static const int kDownsampleKernel[6];

private:
	cv::Mat m_image;
	cv::Mat m_mask;
};

int distance_masked_images(
    const MaskedImage &source, int ys, int xs,
    const MaskedImage &target, int yt, int xt,
    int patch_size
);
