#pragma once

#include <algorithm>
#include <cmath>

namespace menu::ui::motion
{
    inline float smooth(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    inline float out_cubic(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    inline float out_back(float t, float overshoot = 1.6f)
    {
        t = std::clamp(t, 0.0f, 1.0f) - 1.0f;
        return 1.0f + t * t * ((overshoot + 1.0f) * t + overshoot);
    }

    inline float window(float elapsed, float start, float duration)
    {
        if (elapsed < 0.0f)
            return 0.0f;
        return std::clamp((elapsed - start) / duration, 0.0f, 1.0f);
    }

    struct spring
    {
        float value = 0.0f;
        float velocity = 0.0f;
        bool settled = false;

        void snap(float target)
        {
            value = target;
            velocity = 0.0f;
            settled = true;
        }

        float update(float target, float dt, float stiffness = 260.0f, float damping = 22.0f)
        {
            if (!settled)
            {
                snap(target);
                return value;
            }
            dt = std::min(dt, 1.0f / 30.0f);
            velocity += (target - value) * stiffness * dt;
            velocity *= std::exp(-damping * dt);
            value += velocity * dt;
            return value;
        }
    };
}
