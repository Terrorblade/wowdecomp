#include "resolve-shape-geometry.h"

#if defined(_WIN32)

#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include "../core/Contour.h"
#include "../core/Vector2.hpp"
#include "../core/edge-segments.h"

using Microsoft::WRL::ComPtr;

namespace msdfgen {

class ShapeSink : public ID2D1SimplifiedGeometrySink {
public:
    ShapeSink(Shape& shape) : shape(shape), currentContour(nullptr), currentPoint{0, 0} {}

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(ID2D1SimplifiedGeometrySink) || riid == __uuidof(IUnknown)) {
            *ppv = static_cast<ID2D1SimplifiedGeometrySink*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return 1; }
    STDMETHOD_(ULONG, Release)() override { return 1; }

    // ID2D1SimplifiedGeometrySink methods
    STDMETHOD_(void, SetFillMode)(D2D1_FILL_MODE fillMode) override {}
    STDMETHOD_(void, SetSegmentFlags)(D2D1_PATH_SEGMENT vertexFlags) override {}

    STDMETHOD_(void, BeginFigure)(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN figureBegin) override {
        currentContour = &shape.addContour();
        currentPoint = startPoint;
    }

    STDMETHOD_(void, AddLines)(const D2D1_POINT_2F* points, UINT32 pointsCount) override {
        for (UINT32 i = 0; i < pointsCount; ++i) {
            currentContour->addEdge(EdgeHolder(
                Point2(currentPoint.x, currentPoint.y),
                Point2(points[i].x, points[i].y)
            ));
            currentPoint = points[i];
        }
    }

    STDMETHOD_(void, AddBeziers)(const D2D1_BEZIER_SEGMENT* beziers, UINT32 beziersCount) override {
        for (UINT32 i = 0; i < beziersCount; ++i) {
            currentContour->addEdge(EdgeHolder(
                Point2(currentPoint.x, currentPoint.y),
                Point2(beziers[i].point1.x, beziers[i].point1.y),
                Point2(beziers[i].point2.x, beziers[i].point2.y),
                Point2(beziers[i].point3.x, beziers[i].point3.y)
            ));
            currentPoint = beziers[i].point3;
        }
    }

    STDMETHOD_(void, EndFigure)(D2D1_FIGURE_END figureEnd) override {
        // Direct2D's Simplify can sometimes produce empty contours or open ones
        if (currentContour && currentContour->edges.empty()) {
            shape.contours.pop_back();
        }
    }

    STDMETHODIMP Close() override { return S_OK; }

private:
    Shape& shape;
    Contour* currentContour;
    D2D1_POINT_2F currentPoint;
};

bool resolveShapeGeometry(Shape& shape) {
    static ComPtr<ID2D1Factory> factory;
    if (!factory) {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf()))) {
            return false;
        }
    }

    ComPtr<ID2D1PathGeometry> pathGeometry;
    if (FAILED(factory->CreatePathGeometry(&pathGeometry))) {
        return false;
    }

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(pathGeometry->Open(&sink))) {
        return false;
    }

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    for (const auto& contour : shape.contours) {
        if (contour.edges.empty()) continue;

        Point2 p = contour.edges.back()->point(1);
        sink->BeginFigure(D2D1::Point2F((float)p.x, (float)p.y), D2D1_FIGURE_BEGIN_FILLED);

        for (const auto& edge : contour.edges) {
            const Point2* cp = edge->controlPoints();
            switch (edge->type()) {
            case (int)LinearSegment::EDGE_TYPE:
                sink->AddLine(D2D1::Point2F((float)cp[1].x, (float)cp[1].y));
                break;
            case (int)QuadraticSegment::EDGE_TYPE:
                sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(
                    D2D1::Point2F((float)cp[1].x, (float)cp[1].y),
                    D2D1::Point2F((float)cp[2].x, (float)cp[2].y)
                ));
                break;
            case (int)CubicSegment::EDGE_TYPE:
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F((float)cp[1].x, (float)cp[1].y),
                    D2D1::Point2F((float)cp[2].x, (float)cp[2].y),
                    D2D1::Point2F((float)cp[3].x, (float)cp[3].y)
                ));
                break;
            }
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }

    if (FAILED(sink->Close())) {
        return false;
    }

    ComPtr<ID2D1PathGeometry> simplifiedGeometry;
    if (FAILED(factory->CreatePathGeometry(&simplifiedGeometry))) {
        return false;
    }

    ComPtr<ID2D1GeometrySink> simplifiedSink;
    if (FAILED(simplifiedGeometry->Open(&simplifiedSink))) {
        return false;
    }

    if (FAILED(pathGeometry->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES, nullptr, simplifiedSink.Get()))) {
        return false;
    }
    simplifiedSink->Close();

    // Now read back from simplifiedGeometry
    Shape result;
    ShapeSink shapeSink(result);
    if (SUCCEEDED(simplifiedGeometry->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES, nullptr, &shapeSink))) {
        shape = (Shape&&)result;
        shape.orientContours();
        return true;
    }

    return false;
}

} // namespace msdfgen

#else

namespace msdfgen {

// No Direct2D off Windows, so intersections are left alone and only the winding is made consistent.
bool resolveShapeGeometry(Shape &shape) {
    shape.orientContours();
    return false;
}

} // namespace msdfgen

#endif
