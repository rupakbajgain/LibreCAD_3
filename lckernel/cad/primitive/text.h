#pragma once

#include "cad/const.h"
#include "cad/interface/entitydispatch.h"


#include "cad/geometry/geocoordinate.h"
#include "cad/base/cadentity.h"
#include "cad/vo/entitycoordinate.h"
#include "cad/math/lcmath.h"
#include "cad/primitive/textconst.h"
#include <cad/primitive/point.h>
#include "cad/interface/draggable.h"
#include <cad/builders/text.h>

namespace lc {
namespace entity {
class Text : public std::enable_shared_from_this<Text>, public CADEntity, virtual public Visitable, public Draggable {
    friend class builder::TextBuilder;

public:
    /**
    * @brief Text, default constructor
    * @param geo::Coordinate insertion_point, insertion_point of the text
    * @param double height, height of the text
    * @param string text_value, the text itself
    * @param double angle, angle of obliqueness
    * @param string style, name of text style
    * @param TextConst::DrawingDirection textgeneration, Text drawing direction
    * @param TextConst::HAlign halign, Horizontal alignment
    * @param TextConst::VAlign valign, Vertical alignment
    * @param Layer_CSPtr layer, Layer of the entity
    * @param metatypes metatypes of the cad entity
    */
    Text(geo::Coordinate insertion_point,
         std::string text_value,
         double height,
         double angle,
         std::string style,
         const TextConst::DrawingDirection textgeneration,
         const TextConst::HAlign halign,
         const TextConst::VAlign valign,
         bool underlined,
         bool strikethrough,
         bool bold,
         bool italic,
         meta::Layer_CSPtr layer,
         meta::MetaInfo_CSPtr metaInfo = nullptr,
         meta::Block_CSPtr block = nullptr
        );

    Text(const Text_CSPtr &other, bool sameID);

private:
    Text(const builder::TextBuilder& builder);

public:
    const geo::Coordinate _insertion_point;
    const std::string _text_value;
    const double _height;
    const double _angle;
    const std::string _style;
    const TextConst::DrawingDirection _textgeneration;
    const TextConst::VAlign _valign;
    const TextConst::HAlign _halign;
    const bool _underlined;
    const bool _strikethrough;
    const bool _bold;
    const bool _italic;

    geo::Coordinate insertion_point() const {
        return _insertion_point;
    }

    std::string const &text_value() const {
        return _text_value;
    }

    double const angle() const {
        return _angle;
    }

    std::string const &style() const {
        return _style;
    }

    TextConst::DrawingDirection const &textgeneration() const {
        return _textgeneration;
    }

    TextConst::VAlign const &valign() const {
        return _valign;
    }

    TextConst::HAlign const &halign() const {
        return _halign;
    }

    double height() const {
        return _height;
    }

    bool underlined() const {
        return _underlined;
    }

    bool strikethrough() const {
        return _strikethrough;
    }

    bool bold() const {
        return _bold;
    }

    bool italic() const {
        return _italic;
    }

    /**
    * @brief move, moves by an offset
    * @param geo::Coordinate offset
    * @return CADEntity_CSPtr moved entity
    */
public:
    // Methods from CADEntity
    CADEntity_CSPtr move(const geo::Coordinate &offset) const override;
    CADEntity_CSPtr copy(const geo::Coordinate &offset) const override;
    CADEntity_CSPtr rotate(const geo::Coordinate &rotation_center, double rotation_angle) const override;
    CADEntity_CSPtr scale(const geo::Coordinate &scale_center,
                          const geo::Coordinate &scale_factor) const override;
    CADEntity_CSPtr mirror(const geo::Coordinate& axis1, const geo::Coordinate& axis2) const override {
        return NULL; /// @todo no return statement, not implemented in lwpolyline.cpp
    }
    const geo::Area boundingBox() const override;
    CADEntity_CSPtr modify(meta::Layer_CSPtr layer, meta::MetaInfo_CSPtr metaInfo, meta::Block_CSPtr block) const override;
    void accept(GeoEntityVisitor &v) const override {
        v.visit(*this);
    }
    void dispatch(EntityDispatch &ed) const override {
        ed.visit(shared_from_this());
    }
    std::map<unsigned int, lc::geo::Coordinate> dragPoints() const override;
    CADEntity_CSPtr setDragPoints(std::map<unsigned int, lc::geo::Coordinate> dragPoints) const override;

    PropertiesMap availableProperties() const override;

    CADEntity_CSPtr setProperties(const PropertiesMap& propertiesMap) const override;
};

DECLARE_SHORT_SHARED_PTR(Text)
}
}

