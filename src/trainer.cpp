/**
This file is part of Active Appearance Models (AAM).

Copyright Christoph Heindl 2015
Copyright Sebastian Zambal 2015

AAM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

AAM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AAM.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <aam/trainer.h>
#include <aam/model.h>
#include <aam/delaunay.h>
#include <aam/procrustes.h>
#include <aam/pca.h>
#include <aam/rasterization.h>
#include <aam/map.h>

namespace aam {
    
    Trainer::Trainer(const TrainingSet& trainingSet) 
        :_ts(trainingSet)
    {
        eigen_assert(trainingSet.triangles.array().size() > 0);
    }

    /** shift centroid to origin and scale to 0/1 */
    void Trainer::normalizeShape(Eigen::Ref<MatrixX> shape, Eigen::Ref<RowVectorX> weights, Scalar& scaling) const {
        // Convert points from interleaved to x,y per row.
        MatrixX points = fromInterleaved<Scalar>(shape);

        // Center data
        RowVector2 mean = points.colwise().mean();
        points.rowwise() -= mean;

        // Scale to unit
        RowVector2 minC = points.colwise().minCoeff();
        RowVector2 maxC = points.colwise().maxCoeff();
        RowVector2 dia = maxC - minC;
        scaling = dia.maxCoeff();
        points *= Scalar(1) / scaling;
        weights *= Scalar(1) / scaling;

        shape = toInterleaved<Scalar>(points);
    }

    void Trainer::train(ActiveAppearanceModel& model) {

        cv::Mat alignedShapes = _ts.shapes.clone();

        Scalar distance = generalizedProcrustes(toEigenHeader<Scalar>(alignedShapes), 10);

        computePCA(
            toEigenHeader<aam::Scalar>(alignedShapes),
            model.shapeMean, 
            model.shapeModes,
            model.shapeModeWeights);

        model.triangleIndices = _ts.triangles;
        model.barycentricSamplePositions = rasterizeShape(
            model.shapeMean, 
            model.triangleIndices, 
            _ts.images.front().cols, 
            _ts.images.front().rows, 
            1);

        cv::Mat scalarImage;
        cv::Mat colorSamples;
        MatrixX appearances(_ts.shapes.rows, model.barycentricSamplePositions.rows());
        for (size_t i = 0; i < _ts.images.size(); ++i) {            
            _ts.images[i].convertTo(scalarImage, cv::DataType<Scalar>::depth);
            
            readShapeImage(
                toEigenHeader<Scalar>(_ts.shapes.row(i)), // Use orignal shapes here.
                model.triangleIndices, 
                model.barycentricSamplePositions, 
                1, 
                scalarImage,
                colorSamples);

            appearances.row(i) = toEigenHeader<Scalar>(colorSamples).transpose().row(0);
        }

        computePCA(
            appearances,
            model.appearanceMean,
            model.appearanceModes,
            model.appearanceModeWeights);

        // shape auf 0/1 normalisieren
        normalizeShape(model.shapeMean, model.shapeModeWeights, model.shapeScaleToTrainingSize);

    }

    void Trainer::createTriangulation(TrainingSet& trainingSet) {

        ActiveAppearanceModel model;

        aam::Scalar distance = aam::generalizedProcrustes(aam::toEigenHeader<aam::Scalar>(trainingSet.shapes), 10);

        aam::computePCA(aam::toEigenHeader<aam::Scalar>(trainingSet.shapes), model.shapeMean, model.shapeModes, model.shapeModeWeights);

        trainingSet.triangles = aam::findDelaunayTriangulation(model.shapeMean);
    }

}