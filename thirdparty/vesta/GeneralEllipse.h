/*
 * $Revision: 565 $ $Date: 2011-02-15 16:00:43 -0800 (Tue, 15 Feb 2011) $
 *
 * Copyright by Astos Solutions GmbH, Germany
 *
 * this file is published under the Astos Solutions Free Public License
 * For details on copyright and terms of use see
 * http://www.astos.de/Astos_Solutions_Free_Public_License.html
 */

#ifndef _VESTA_GENERAL_ELLIPSE_H_
#define _VESTA_GENERAL_ELLIPSE_H_

#include <Eigen/Core>
#include <Eigen/Geometry>


namespace vesta
{

/** GeneralEllipse represents an arbitrary ellipse in 3D space. The ellipse
  * is defined by a center point C and two generating vectors v0 and v1. The
  * ellipse is the set of points:
  *
  * C + cos(theta) v0 + sin(theta) v1
  */
class GeneralEllipse
{
public:
    /** Create an ellipse with the specified center and generating vectors.
      */
    GeneralEllipse(const Eigen::Vector3d& center,
                   const Eigen::Vector3d& v0,
                   const Eigen::Vector3d& v1) :
        m_center(center)
    {
        m_generatingVectors.row(0) = v0;
        m_generatingVectors.row(1) = v1;
    }

    Eigen::Vector3d center() const
    {
        return m_center;
    }

    Eigen::Matrix<double, 2, 3> generatingVectors() const
    {
        return m_generatingVectors;
    }

    Eigen::Matrix<double, 2, 3> principalSemiAxes() const;

private:
    Eigen::Vector3d m_center;
    Eigen::Matrix<double, 2, 3> m_generatingVectors;
};

}

#endif // _VESTA_GENERAL_ELLIPSE_H_
