%module jtriskel

%include "std_string.i"
%include "std_vector.i"
%include "std_unique_ptr.i"

%unique_ptr(triskel::CFGLayout)
%unique_ptr(triskel::LayoutBuilder)
%unique_ptr(triskel::ExportingRenderer)

%{
#include "triskel/triskel.hpp"
%}

namespace triskel {
    struct Point;
}

%template(PointVector) std::vector<triskel::Point>;

%ignore triskel::CFGLayout::render_and_save(ExportingRenderer&, const std::filesystem::path&);

%include "triskel/triskel.hpp"
