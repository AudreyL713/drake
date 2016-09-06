#!/usr/bin/env python

"""Generate c++ and LCM definitions for the LCM Vector concept.
"""

import argparse
import os
import subprocess


def put(fileobj, text, newlines_after=0):
    fileobj.write(text.strip('\n') + '\n' * newlines_after)


INDICES_BEGIN = """
/// Describes the row indices of a %(camel)s.
struct DRAKECARS_EXPORT %(indices)s {
  /// The total number of rows (coordinates).
  static const int kNumCoordinates = %(nfields)d;

  // The index of each individual coordinate.
"""
INDICES_FIELD = """static const int %(kname)s = %(k)d;"""
INDICES_FIELD_STORAGE = """const int %(indices)s::%(kname)s;"""
INDICES_END = """
};
"""

def to_kname(field):
    return 'k' + ''.join([
        word.capitalize()
        for word in field.split('_')])

def generate_indices(context, fields):
    # pylint: disable=unused-variable
    header = context["header"]
    camel = context["camel"]
    indices = context["indices"]
    nfields = len(fields)
    kname = "kNumCoordinates"
    put(header, INDICES_BEGIN % locals(), 1)
    for k, field in enumerate(fields):
        kname = to_kname(field)
        put(header, INDICES_FIELD % locals(), 1)
    put(header, INDICES_END % locals(), 2)

def generate_indices_storage(context, fields):
    # pylint: disable=unused-variable
    cc = context["cc"]
    camel = context["camel"]
    indices = context["indices"]
    nfields = len(fields)
    kname = "kNumCoordinates"
    put(cc, INDICES_FIELD_STORAGE % locals(), 1)
    for k, field in enumerate(fields):
        kname = to_kname(field)
        put(cc, INDICES_FIELD_STORAGE % locals(), 1)
    put(cc, '', 1)


DEFAULT_CTOR = """
  /// Default constructor.  Sets all rows to zero.
  %(camel)s() : systems::BasicStateAndOutputVector<T>(K::kNumCoordinates) {
    this->SetFromVector(VectorX<T>::Zero(K::kNumCoordinates));
  }
"""

def generate_default_ctor(context, _):
    header = context["header"]
    put(header, DEFAULT_CTOR % context, 2)


ACCESSOR_BEGIN = """
  /// @name Getters and Setters
  //@{
"""
ACCESSOR = """
    const T %(field)s() const { return this->GetAtIndex(K::%(kname)s); }
    void set_%(field)s(const T& %(field)s) {
      this->SetAtIndex(K::%(kname)s, %(field)s);
    }
"""
ACCESSOR_END = """
  //@}
"""

def generate_accessors(context, fields):
    # pylint: disable=unused-variable
    header = context["header"]
    indices = context["indices"]
    put(header, ACCESSOR_BEGIN % locals(), 1)
    for field in fields:
        kname = to_kname(field)
        put(header, ACCESSOR % locals(), 1)
    put(header, ACCESSOR_END % locals(), 2)

ENCODE_BEGIN = """
template <typename ScalarType>
bool encode(const double& t, const %(camel)s<ScalarType>& wrap,
            // NOLINTNEXTLINE(runtime/references)
            drake::lcmt_%(snake)s_t& msg) {
  msg.timestamp = static_cast<int64_t>(t * 1000);
"""
ENCODE_FIELD = """  msg.%(field)s = wrap.%(field)s();"""
ENCODE_END = """
  return true;
}
"""


def generate_encode(context, fields):
    header = context["header"]
    put(header, ENCODE_BEGIN % context, 1)
    # pylint: disable=unused-variable
    for k, field in enumerate(fields):
        put(header, ENCODE_FIELD % locals(), 1)
    put(header, ENCODE_END % context, 2)


DECODE_BEGIN = """
template <typename ScalarType>
bool decode(const drake::lcmt_%(snake)s_t& msg,
            // NOLINTNEXTLINE(runtime/references)
            double& t,
            // NOLINTNEXTLINE(runtime/references)
            %(camel)s<ScalarType>& wrap) {
  t = static_cast<double>(msg.timestamp) / 1000.0;
"""
DECODE_FIELD = """  wrap.set_%(field)s(msg.%(field)s);"""
DECODE_END = """
  return true;
}
"""


def generate_decode(context, fields):
    header = context["header"]
    put(header, DECODE_BEGIN % context, 1)
    # pylint: disable=unused-variable
    for k, field in enumerate(fields):
        put(header, DECODE_FIELD % locals(), 1)
    put(header, DECODE_END % context, 2)


HEADER_PREAMBLE = """
#pragma once

// This file is generated by a script.  Do not edit!
// See %(generator)s.

#include <stdexcept>
#include <string>

#include <Eigen/Core>

#include "lcmtypes/drake/lcmt_%(snake)s_t.hpp"
#include "drake/drakeCars_export.h"
#include "drake/systems/framework/basic_state_and_output_vector.h"

namespace drake {
namespace cars {
"""

CLASS_BEGIN = """

/// Specializes BasicStateAndOutputVector with specific getters and setters.
template <typename T>
class %(camel)s : public systems::BasicStateAndOutputVector<T> {
 public:
  // An abbreviation for our row index constants.
  typedef %(indices)s K;
"""

CLASS_END = """
  /// @name Implement the LCMVector concept
  //@{
  typedef drake::lcmt_%(snake)s_t LCMMessageType;
  static std::string channel() { return "%(screaming_snake)s"; }
  //@}
};
"""

HEADER_POSTAMBLE = """
}  // namespace cars
}  // namespace drake
"""

CC_PREAMBLE = """
#include "drake/examples/Cars/gen/%(snake)s.h"

// This file is generated by a script.  Do not edit!
// See %(generator)s.

namespace drake {
namespace cars {
"""

CC_POSTAMBLE = """
}  // namespace cars
}  // namespace drake
"""

LCMTYPE_PREAMBLE = """
// This file is generated by %(generator)s. Do not edit.
package drake;

struct lcmt_%(snake)s_t
{
  int64_t timestamp;

"""

LCMTYPE_POSTAMBLE = """
}
"""


def generate_code(args):
    # pylint: disable=unused-variable
    drake_dist_dir = subprocess.check_output(
        "git rev-parse --show-toplevel".split()).strip()
    generator = os.path.abspath(__file__).replace(
        os.path.join(drake_dist_dir, ''), '')
    title_phrase = args.title.split()
    camel = ''.join([x.capitalize() for x in title_phrase])
    indices = camel + 'Indices'
    snake = '_'.join([x.lower() for x in title_phrase])
    screaming_snake = '_'.join([x.upper() for x in title_phrase])
    header_file = os.path.abspath(
        os.path.join(args.header_dir, "%s.h" % snake))
    cc_file = os.path.abspath(
        os.path.join(args.header_dir, "%s.cc" % snake))
    lcm_file = os.path.abspath(
        os.path.join(args.lcmtype_dir, "lcmt_%s_t.lcm" % snake))

    header = open(header_file, 'w')
    cc = open(cc_file, 'w')
    lcmtype = open(lcm_file, 'w')

    put(header, HEADER_PREAMBLE % locals(), 2)
    generate_indices(locals(), args.fields)
    put(header, CLASS_BEGIN % locals(), 2)
    generate_default_ctor(locals(), args.fields)
    generate_accessors(locals(), args.fields)
    put(header, CLASS_END % locals(), 2)
    generate_encode(locals(), args.fields)
    generate_decode(locals(), args.fields)
    put(header, HEADER_POSTAMBLE % locals(), 1)

    put(cc, CC_PREAMBLE % locals(), 2)
    generate_indices_storage(locals(), args.fields)
    put(cc, CC_POSTAMBLE % locals(), 1)

    put(lcmtype, LCMTYPE_PREAMBLE % locals(), 1)
    for field in args.fields:
        put(lcmtype, "  double %s;" % field, 1)
    put(lcmtype, LCMTYPE_POSTAMBLE % locals(), 1)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        '--header-dir', help="output directory for header file", default=".")
    parser.add_argument(
        '--lcmtype-dir', help="output directory for lcm file", default=".")
    parser.add_argument(
        '--title', help="title phrase, from which type names will be made")
    parser.add_argument(
        'fields', metavar='FIELD', nargs='+', help="field names for vector")
    args = parser.parse_args()
    generate_code(args)

if __name__ == "__main__":
    main()
