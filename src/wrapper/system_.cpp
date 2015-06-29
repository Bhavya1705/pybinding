#include "system/System.hpp"
#include "system/Shape.hpp"
#include "system/Lattice.hpp"
#include "system/Symmetry.hpp"
#include "system/SystemModifiers.hpp"

#include <boost/python/class.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/pure_virtual.hpp>
#include "python_support.hpp"
using namespace boost::python;


class PySiteStateModifier : public tbm::SiteStateModifier, public wrapper<tbm::SiteStateModifier> {
public:
    virtual void apply(ArrayX<bool>& is_valid, const CartesianArray& p) const override {
        object o = get_override("apply")(is_valid, p.x, p.y, p.z);
        is_valid = extract<ArrayX<bool>>(o);
    }
    void apply_dummy(ArrayX<bool>&, const ArrayXf&, const ArrayXf&, const ArrayXf&) const {}
};

class PyPositionModifier : public tbm::PositionModifier, public wrapper<tbm::PositionModifier> {
public:
    virtual void apply(CartesianArray& p) const override {
        tuple o = get_override("apply")(p.x, p.y, p.z);
        p.x = extract<ArrayXf>(o[0]);
        p.y = extract<ArrayXf>(o[1]);
        p.z = extract<ArrayXf>(o[2]);
    }
    void apply_dummy(ArrayXf&, ArrayXf&, ArrayXf&) const {}
};

void export_system()
{
    using tbm::System;
    using Boundary = tbm::System::Boundary;

    class_<Boundary>{"Boundary", no_init}
    .add_property("shift", copy_value(&Boundary::shift))
    .add_property("matrix", sparse_uref(&Boundary::matrix))
    ;

    class_<System>{"System", no_init}
    .def("find_nearest", &System::find_nearest, args("self", "position", "sublattice"_kw=-1),
         "Find the index of the atom closest to the given coordiantes.")
    .add_property("num_sites", &System::num_sites)
    .add_property("positions", internal_ref(&System::positions))
    .add_property("sublattice", dense_uref(&System::sublattice))
    .add_property("boundaries", &System::boundaries)
    .add_property("matrix", sparse_uref(&System::matrix))
    ;

    using tbm::Hopping;
    class_<Hopping>{"Hopping"}
    .add_property("relative_index", copy_value(&Hopping::relative_index))
    .def_readonly("to_sublattice", &Hopping::to_sublattice)
    .def_readonly("energy", &Hopping::energy)
    .enable_pickling()
    .def("__getstate__", [](Hopping const& h) {
        return make_tuple(h.relative_index, h.to_sublattice, h.energy);
    })
    .def("__setstate__", [](Hopping& h, tuple t) {
        h = {extract<decltype(h.relative_index)>(t[0]), extract<decltype(h.to_sublattice)>(t[1]),
             extract<decltype(h.energy)>(t[2])};
    })
    ;

    using tbm::Sublattice;
    class_<Sublattice>{"Sublattice"}
    .add_property("offset", copy_value(&Sublattice::offset))
    .def_readonly("onsite", &Sublattice::onsite)
    .def_readonly("alias", &Sublattice::alias)
    .add_property("hoppings", &Sublattice::hoppings)
    .enable_pickling()
    .def("__getstate__", [](Sublattice const& s) {
        return make_tuple(s.offset, s.onsite, s.alias, s.hoppings);
    })
    .def("__setstate__", [](Sublattice& s, tuple t) {
        s = {extract<decltype(s.offset)>(t[0]), extract<decltype(s.onsite)>(t[1]),
             extract<decltype(s.alias)>(t[2]), extract<decltype(s.hoppings)>(t[3])};
    })
    ;

    using tbm::Lattice;
    class_<Lattice, noncopyable>{
        "Lattice", init<int>{args("self", "min_neighbors"_kw=1)}
    }
    .def("add_vector", &Lattice::add_vector, args("self", "primitive_vector"))
    .def("create_sublattice", &Lattice::create_sublattice,
         args("self", "offset", "onsite_potential"_kw=.0f, "alias"_kw=-1))
    .def("add_hopping", &Lattice::add_hopping,
         args("self", "relative_index", "from_sublattice", "to_sublattice", "hopping_energy"))
    .add_property("vectors", &Lattice::vectors, &Lattice::vectors)
    .add_property("sublattices", &Lattice::sublattices, [](Lattice& l, std::vector<Sublattice> s) {
        l.has_onsite_potential = std::any_of(s.begin(), s.end(), [](Sublattice const& sub) {
            return sub.onsite != 0;
        });
        l.sublattices = std::move(s);
    })
    .def_readwrite("min_neighbors", &Lattice::min_neighbours)
    .enable_pickling()
    ;
    
    using tbm::Shape;
    class_<Shape, noncopyable> {"Shape", no_init};
    
    using tbm::Primitive;
    class_<Primitive, bases<Shape>, noncopyable> {
        "Primitive", "Shape of the primitive unit cell",
        init<Cartesian, bool> {(arg("self"), "length", arg("nanometers")=false)}
    };

    using tbm::Circle;
    class_<Circle, bases<Shape>, noncopyable> {
        "Circle", "Perfect circle",
        init<float, optional<Cartesian>> {
            (arg("self"), "radius", "center")
        }
    }
    .add_property("r", &Circle::radius, &Circle::radius)
    .add_property("center", &Circle::_center, &Circle::_center)
    ;
    
    using tbm::Polygon;
    class_<Polygon, bases<Shape>, noncopyable> {
        "Polygon", "Shape defined by a list of vertices", init<> {arg("self")}
    }
    .add_property("x", copy_value(&Polygon::x), &Polygon::x)
    .add_property("y", copy_value(&Polygon::y), &Polygon::y)
    .add_property("offset", copy_value(&Polygon::offset), &Polygon::offset)
    ;

    using tbm::Symmetry;
    class_<Symmetry, noncopyable> {"Symmetry", no_init};

    using tbm::Translational;
    class_<Translational, bases<Symmetry>, noncopyable> {
        "Translational", "Periodic boundary condition.", 
        init<Cartesian> {(arg("self"), "length")}
    };

    class_<PySiteStateModifier, noncopyable>{"SiteStateModifier"}
    .def("apply", pure_virtual(&PySiteStateModifier::apply_dummy), args("self", "site_state", "x", "y", "z"))
    ;
    class_<PyPositionModifier, noncopyable>{"PositionModifier"}
    .def("apply", pure_virtual(&PyPositionModifier::apply_dummy), args("self", "x", "y", "z"))
    ;
}
