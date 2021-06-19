#pragma once

#include <glm/glm.hpp>


namespace dal {

    class ILight {

    public:
        glm::vec3 m_pos;
        glm::vec3 m_color;

    };


    class DLight : public ILight {

    private:
        glm::vec3 m_direc_to_light;

    public:
        auto& to_light_direc() const {
            return this->m_direc_to_light;
        }

        void set_direc_to_light(const glm::vec3& v) {
            this->m_direc_to_light = glm::normalize(v);
        }

        void set_direc_to_light(const float x, const float y, const float z) {
            this->set_direc_to_light(glm::vec3{x, y, z});
        }

    };


    class PLight : public ILight {

    public:
        double m_max_dist = 20;

    };


    class SLight : public DLight {

    public:
        glm::vec3 m_direc;
        double m_max_dist = 20;

    private:
        double m_fade_start;
        double m_fade_end;
        double m_fade_end_radians;

    public:
        auto fade_start() const {
            return this->m_fade_start;
        }

        auto fade_end() const {
            return this->m_fade_end;
        }

        auto fade_end_radians() const {
            return this->m_fade_end_radians;
        }

        void set_fade_start_degree(const double degree) {
            this->m_fade_start = std::cos(glm::radians(degree));
        }

        void set_fade_end_degree(const double degree) {
            this->m_fade_end_radians = glm::radians(degree);
            this->m_fade_end = std::cos(this->m_fade_end_radians);
        }

    };

}