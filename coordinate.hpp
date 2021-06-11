/**
 * @file coordinate.hpp
 *
 * @author Tnze <cjd001113@outlook.com>
 */
#include <cmath>

template <typename T>
class Vec2
{
public:
    T x, y;
    Vec2<T> &operator+=(const Vec2<T> rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
};

typedef Vec2<float> Vec2f;

class PositionStatus
{
    void xy_to_mn(float sx, float sy, int &sm, int &sn)
    {
        sm = (int)(sqrtf(powf(-R - sx, 2) + sy * sy) / sl);
        sn = (int)(sqrtf(powf(R - sx, 2) + sy * sy) / sl);
    }

    void mn_to_xy(int sm, int sn, float &sx, float &sy)
    {
        float fm = sm * sl;
        float fn = sn * sl;
        sx = (fm * fm - fn * fn) / (4 * R);
        // sy = sqrtf((fn - sx + R) * (fn + sx - R)); // or
        sy = sqrtf((fm - sx - R) * (fm + sx + R));
    }

public:
    float sl;
    float R;
    int m, n;
    PositionStatus(float step_len, float D, int m, int n) : sl(step_len), R(D / 2), m(m), n(n) {}

    void Pos(float &x, float &y, float &M, float &N)
    {
        mn_to_xy(m, n, x, y);
        M = m * sl;
        N = n * sl;
    }

    void MoveTo(float x, float y, int &dm, int &dn)
    {
        dm = m;
        dn = n;
        xy_to_mn(x, y, m, n);
        dm = m - dm;
        dn = n - dn;
    }
};