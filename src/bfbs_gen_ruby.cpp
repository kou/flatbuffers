/*
 * Copyright 2025 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bfbs_gen_ruby.h"

#include <iostream>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// Ensure no includes to flatc internals. bfbs_gen.h and generator.h are OK.
#include "bfbs_gen.h"
#include "bfbs_namer.h"

// The intermediate representation schema.
#include "flatbuffers/code_generator.h"
#include "flatbuffers/reflection.h"
#include "flatbuffers/reflection_generated.h"

namespace flatbuffers {
namespace {

// To reduce typing
namespace r = ::reflection;

std::set<std::string> RubyKeywords() {
  return {
    "__ENCODING__",
    "__LINE__",
    "__FILE__",
    "BEGIN",
    "END",
    "alias",
    "and",
    "begin",
    "break",
    "case",
    "class",
    "def",
    "defined?",
    "do",
    "else",
    "elsif",
    "end",
    "ensure",
    "false",
    "for",
    "if",
    "in",
    "module",
    "next",
    "nil",
    "not",
    "or",
    "redo",
    "rescue",
    "retry",
    "return",
    "self",
    "super",
    "then",
    "true",
    "undef",
    "unless",
    "until",
    "when",
    "while",
    "yield",
  };
}

Namer::Config RubyDefaultConfig() {
  return { /*types=*/Case::kUpperCamel,
           /*constants=*/Case::kUpperCamel,
           /*methods=*/Case::kSnake,
           /*functions=*/Case::kSnake,
           /*fields=*/Case::kSnake,
           /*variables=*/Case::kSnake,
           /*variants=*/Case::kScreamingSnake,
           /*enum_variant_seperator=*/"::",
           /*escape_keywords=*/Namer::Config::Escape::AfterConvertingCase,
           /*namespaces=*/Case::kUpperCamel,
           /*namespace_seperator=*/"::",
           /*object_prefix=*/"op",
           /*object_suffix=*/"os",
           /*keyword_prefix=*/"kp",
           /*keyword_suffix=*/"ks",
           /*filenames=*/Case::kSnake,
           /*directories=*/Case::kSnake,
           /*output_path=*/"",
           /*filename_suffix=*/"",
           /*filename_extension=*/".rb" };
}

class RubyBfbsGenerator : public BaseBfbsGenerator {
 public:
  explicit RubyBfbsGenerator(const std::string &flatc_version)
      : BaseBfbsGenerator(),
        requires_(),
        current_obj_(nullptr),
        current_enum_(nullptr),
        flatc_version_(flatc_version),
        namer_(RubyDefaultConfig(), RubyKeywords()),
        indent_("") {}

  Status GenerateFromSchema(const r::Schema *schema,
                            const CodeGenOptions &options)
      override {
    options_ = options;
    if (!GenerateEnums(schema->enums())) { return ERROR; }
    if (!GenerateObjects(schema->objects(), schema->root_table())) {
      return ERROR;
    }
    return OK;
  }

  using BaseBfbsGenerator::GenerateCode;

  Status GenerateCode(const Parser &, const std::string &,
                      const std::string &) override {
    return Status::NOT_IMPLEMENTED;
  }

  Status GenerateMakeRule(const Parser &parser, const std::string &path,
                          const std::string &filename,
                          std::string &output) override {
    (void)parser;
    (void)path;
    (void)filename;
    (void)output;
    return Status::NOT_IMPLEMENTED;
  }

  Status GenerateGrpcCode(const Parser &parser, const std::string &path,
                          const std::string &filename) override {
    (void)parser;
    (void)path;
    (void)filename;
    return Status::NOT_IMPLEMENTED;
  }

  Status GenerateRootFile(const Parser &parser,
                          const std::string &path) override {
    (void)parser;
    (void)path;
    return Status::NOT_IMPLEMENTED;
  }

  bool IsSchemaOnly() const override { return true; }

  bool SupportsBfbsGeneration() const override { return true; }

  bool SupportsRootFileGeneration() const override { return false; }

  IDLOptions::Language Language() const override { return IDLOptions::kRuby; }

  std::string LanguageName() const override { return "Ruby"; }

  uint64_t SupportedAdvancedFeatures() const override {
    return 0xF;
  }

 private:
  void Indent() {
    indent_ += "  ";
  }

  void Unindent() {
    indent_ = indent_.substr(0, indent_.size() - 2);
  }

  std::vector<std::string> SplitNamespace(const std::string &ns) {
    std::vector<std::string> components;
    if (ns.empty()) {
      return components;
    }

    // We can use std::views::split() with C++20.
    std::string::size_type start = 0;
    while (true) {
      auto next_start = ns.find_first_of(".", start);
      components.push_back(ns.substr(start, next_start - start));
      if (next_start == std::string::npos) { break; }
      start = next_start + 1;
    }
    return components;
  }

  bool IsEnumType(const r::Type *type) {
    const auto base_type = type->base_type();
    if (IsInteger(base_type)) {
      return type->index() >= 0;
    } else {
      return false;
    }
  }

  bool GenerateEnums(
      const flatbuffers::Vector<flatbuffers::Offset<r::Enum>> *enums) {
    ForAllEnums(enums, [&](const r::Enum *enum_def) {
      std::string code;

      StartCodeBlock(enum_def);

      std::string ns;
      const auto enum_name =
        namer_.Type(namer_.Denamespace(enum_def, ns));
      const auto ns_components = SplitNamespace(ns);

      for (const auto &ns_component : ns_components) {
        code += indent_ + "module " + namer_.Namespace(ns_component) + "\n";
        Indent();
      }

      GenerateDocumentation(enum_def->documentation(), code);
      if (enum_def->is_union()) {
        code += indent_ + "class " + enum_name + " < ::FlatBuffers::Union\n";
      } else {
        const auto attributes = enum_def->attributes();
        if (attributes && attributes->LookupByKey("bit_flags")) {
          code += indent_ + "class " + enum_name + " < ::FlatBuffers::Flags\n";
        } else {
          code += indent_ + "class " + enum_name + " < ::FlatBuffers::Enum\n";
        }
      }
      Indent();
      ForAllEnumValues(enum_def, [&](const reflection::EnumVal *enum_val) {
        GenerateDocumentation(enum_val->documentation(), code);
        if (enum_def->is_union()) {
          auto [path, table_class_name] = ResolveClassName(enum_val->union_type(),
                                                           ns_components,
                                                           false,
                                                           true);
          code += indent_ + namer_.Variant(enum_val->name()->str()) +
            " = register(\"" + enum_val->name()->str() + "\", " +
            NumToString(enum_val->value()) + ", " +
            (table_class_name ? "\"" + *table_class_name + "\"" : "nil") + ", " + (path ? "\"" + *path + "\"" : "nil") +
            ")\n";
        } else {
          code += indent_ + namer_.Variant(enum_val->name()->str()) +
            " = register(\"" + enum_val->name()->str() + "\", " +
            NumToString(enum_val->value()) + ")\n";
        }
      });
      if (enum_def->is_union()) {
        code += "\n";
        code += indent_ + "private def require_table_class\n";
        code += indent_ + "  require_relative @require_path\n";
        code += indent_ + "end\n";
      }

      Unindent();
      code += indent_ + "end\n";
      for (const auto &_ : ns_components) {
        Unindent();
        code += indent_ + "end\n";
      }

      EmitCodeBlock(code, enum_name, ns_components, enum_def->declaration_file()->str());
      Unindent();
    });
    return true;
  }

  bool GenerateObjects(
      const flatbuffers::Vector<flatbuffers::Offset<r::Object>> *objects,
      const r::Object *root_object) {
    (void)root_object;
    ForAllObjects(objects, [&](const r::Object *object) {
      std::string code;

      StartCodeBlock(object);

      std::string ns;
      const auto object_name =
          namer_.Type(namer_.Denamespace(object, ns));
      auto ns_components = SplitNamespace(ns);

      for (const auto &ns_component : ns_components) {
        code += indent_ + "module " + namer_.Namespace(ns_component) + "\n";
        Indent();
      }

      GenerateDocumentation(object->documentation(), code);

      if (object->is_struct()) {
        code += indent_ + "class " + object_name + " < ::FlatBuffers::Struct\n";
      } else {
        code += indent_ + "class " + object_name + " < ::FlatBuffers::Table\n";
      }
      Indent();
      // code += indent_ + "def initialize\n";
      // code += indent_ + "end\n";
      // code += "\n";

      // if (object == root_object) {
      //   code += "function " + object_name + ".GetRootAs" + object_name +
      //           "(buf, offset)\n";
      //   code += "  if type(buf) == \"string\" then\n";
      //   code += "    buf = flatbuffers.binaryArray.New(buf)\n";
      //   code += "  end\n";
      //   code += "\n";
      //   code += "  local n = flatbuffers.N.UOffsetT:Unpack(buf, offset)\n";
      //   code += "  local o = " + object_name + ".New()\n";
      //   code += "  o:Init(buf, n + offset)\n";
      //   code += "  return o\n";
      //   code += "end\n";
      //   code += "\n";
      // }

      // Generates a init method that receives a pre-existing accessor object,
      // so that objects can be reused.

      // code += "function mt:Init(buf, pos)\n";
      // code += "  self.view = flatbuffers.view.New(buf, pos)\n";
      // code += "end\n";
      // code += "\n";

      // Create all the field accessors.
      size_t n_processed_fields = 0;
      ForAllFields(object, /*reverse=*/false, [&](const r::Field *field) {
        // Skip writing deprecated fields altogether.
        if (field->deprecated()) { return; }

        const auto& field_name = namer_.Field(*field);
        const auto type = field->type();
        const auto base_type = type->base_type();

        if (n_processed_fields > 0) {
          code += "\n";
        }

        GenerateDocumentation(field->documentation(), code);

        if (base_type == r::Bool) {
          std::string predicate = field_name + "?";
          if (predicate.substr(0, 3) == "is_") {
            predicate = predicate.substr(3);
          }
          code += indent_ + "def " + predicate + "\n";
        } else {
          code += indent_ + "def " + field_name + "\n";
        }
        Indent();
        const auto field_offset_string = NumToString(field->offset());
        auto field_offset_direct_code =
          indent_ + "field_offset = " + field_offset_string + "\n";
        std::string field_offset_virtual_code;
        field_offset_virtual_code +=
          indent_ + "field_offset = @view.unpack_virtual_offset(" + field_offset_string + ")\n";
        field_offset_virtual_code +=
          indent_ + "return " + DefaultValue(field) + " if field_offset.zero?\n";
        field_offset_virtual_code += "\n";

        const auto unpack_method_name =
            "@view.unpack_" +
            namer_.Method(std::string(r::EnumNameBaseType(base_type)));
        if (IsScalar(base_type)) {
          if (object->is_struct()) {
            code += field_offset_direct_code;
            if (IsEnumType(type)) {
              code += indent_ + "enum_value = " + unpack_method_name + "(field_offset)\n";
              const auto klass = RegisterRequires(field->type(), ns_components);
              code += indent_ + klass + ".try_convert(enum_value) || enum_value\n";
            } else {
              code += indent_ + unpack_method_name + "(field_offset)\n";
            }
          } else {
            // Table accessors
            if (IsEnumType(type)) {
              const auto klass = RegisterRequires(field->type(), ns_components);
              code += indent_ + "field_offset = @view.unpack_virtual_offset(" + field_offset_string + ")\n";
              code += indent_ + "if field_offset.zero?\n";
              code += indent_ + "  enum_value = " + DefaultValue(field) + "\n";
              code += indent_ + "else\n";
              code += indent_ + "  enum_value = " + unpack_method_name + "(field_offset)\n";
              code += indent_ + "end\n";
              code += indent_ + klass + ".try_convert(enum_value) || enum_value\n";
            } else {
              code += field_offset_virtual_code;
              code += indent_ + unpack_method_name + "(field_offset)\n";
            }
          }
        } else {
          switch (base_type) {
            case r::String: {
              code += field_offset_virtual_code;
              code += indent_ + unpack_method_name + "(field_offset)\n";
              break;
            }
            case r::Obj: {
              if (object->is_struct()) {
                code += field_offset_direct_code;
                const auto klass = RegisterRequires(field->type(), ns_components);
                code += indent_ + "@view.unpack_struct(" + klass + ", field_offset)\n";
              } else {
                code += field_offset_virtual_code;
                const auto field_object = GetObject(type);
                const auto klass = RegisterRequires(field->type(), ns_components);
                if (field_object->is_struct()) {
                  code += indent_ + "@view.unpack_struct(" + klass + ", field_offset)\n";
                } else {
                  code += indent_ + "@view.unpack_table(" + klass + ", field_offset)\n";
                }
              }
              break;
            }
            case r::Union: {
              code += indent_ + "type = " + field_name + "_type\n";
              code += indent_ + "return if type.nil?\n";
              code += "\n";
              code += field_offset_virtual_code;
              code += indent_ +
                "@view.unpack_union(type.table_class, field_offset)\n";
              break;
            }
            case r::Array:
            case r::Vector: {
              const r::BaseType element_base_type = type->element();
              int32_t element_size = type->element_size();
              const auto element_size_string = NumToString(
                element_size);
              std::string unpack_vector_code;
              unpack_vector_code +=
                indent_ + "@view.unpack_vector(field_offset, " +
                element_size_string + ") do |element_offset|\n";
              if (element_base_type == r::Obj) {
                const auto klass = RegisterRequires(field->type(), ns_components, true);
                const auto element_object = GetObjectByIndex(type->index());
                if (element_object->is_struct()) {
                  code += field_offset_virtual_code;
                  code += unpack_vector_code;
                  code += indent_ + "  @view.unpack_struct(" + klass +
                          ", element_offset)\n";
                  code += indent_ + "end\n";
                } else {
                  code += field_offset_virtual_code;
                  code += indent_ + "@view.unpack_vector(field_offset, " +
                    element_size_string + ") do |element_offset|\n";
                  code += indent_ + "  @view.unpack_table(" + klass + ", element_offset)\n";
                  code += indent_ + "end\n";
                }
              } else {
                code += field_offset_virtual_code;
                code += unpack_vector_code;
                std::string element_unpack_method_name;
                element_unpack_method_name += "@view.unpack_" +
                  namer_.Method(std::string(
                                  r::EnumNameBaseType(element_base_type)));
                code += indent_ + "  " + element_unpack_method_name + "(element_offset)\n";
                code += indent_ + "end\n";
              }
              break;
            }
            default: {
              return;
            }
          }
        }
        Unindent();
        code += indent_ + "end\n";

        n_processed_fields++;
      });

      // // Create all the builders
      // if (object->is_struct()) {
      //   code += "function " + object_name + ".Create" + object_name +
      //           "(builder" + GenerateStructBuilderArgs(object) + ")\n";
      //   code += AppendStructBuilderBody(object);
      //   code += "  return builder:Offset()\n";
      //   code += "end\n";
      //   code += "\n";
      // } else {
      //   // Table builders
      //   code += "function " + object_name + ".Start(builder)\n";
      //   code += "  builder:StartObject(" +
      //           NumToString(object->fields()->size()) + ")\n";
      //   code += "end\n";
      //   code += "\n";

      //   ForAllFields(object, /*reverse=*/false, [&](const r::Field *field) {
      //     if (field->deprecated()) { return; }

      //     const std::string field_name = namer_.Field(*field);
      //     const std::string variable_name = namer_.Variable(*field);

      //     code += "function " + object_name + ".Add" + field_name +
      //             "(builder, " + variable_name + ")\n";
      //     code += "  builder:Prepend" + GenerateMethod(field) + "Slot(" +
      //             NumToString(field->id()) + ", " + variable_name + ", " +
      //             DefaultValue(field) + ")\n";
      //     code += "end\n";
      //     code += "\n";

      //     if (IsVector(field->type()->base_type())) {
      //       code += "function " + object_name + ".Start" + field_name +
      //               "Vector(builder, numElems)\n";

      //       const int32_t element_size = field->type()->element_size();
      //       int32_t alignment = 0;
      //       if (IsStruct(field->type(), /*use_element=*/true)) {
      //         alignment = GetObjectByIndex(field->type()->index())->minalign();
      //       } else {
      //         alignment = element_size;
      //       }

      //       code += "  return builder:StartVector(" +
      //               NumToString(element_size) + ", numElems, " +
      //               NumToString(alignment) + ")\n";
      //       code += "end\n";
      //       code += "\n";
      //     }
      //   });

      //   code += "function " + object_name + ".End(builder)\n";
      //   code += "  return builder:EndObject()\n";
      //   code += "end\n";
      //   code += "\n";
      // }

      Unindent();
      code += indent_ + "end\n";

      for (const auto &_ : ns_components) {
        Unindent();
        code += indent_ + "end\n";
      }

      EmitCodeBlock(code, object_name, ns_components, object->declaration_file()->str());
    });
    return true;
  }

 private:
  void GenerateDocumentation(
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>
          *documentation, std::string &code) const {
    flatbuffers::ForAllDocumentation(
        documentation, [&](const flatbuffers::String *str) {
          code += indent_ + "#" + str->str() + "\n";
        });
  }

  std::string GenerateStructBuilderArgs(const r::Object *object,
                                        std::string prefix = "") const {
    std::string signature;
    ForAllFields(object, /*reverse=*/false, [&](const r::Field *field) {
      if (IsStructOrTable(field->type()->base_type())) {
        const r::Object *field_object = GetObject(field->type());
        signature += GenerateStructBuilderArgs(
            field_object, prefix + namer_.Variable(*field) + "_");
      } else {
        signature += ", " + prefix + namer_.Variable(*field);
      }
    });
    return signature;
  }

  std::string AppendStructBuilderBody(const r::Object *object,
                                      std::string prefix = "") const {
    std::string code;
    code += "  builder:Prep(" + NumToString(object->minalign()) + ", " +
            NumToString(object->bytesize()) + ")\n";

    // We need to reverse the order we iterate over, since we build the
    // buffer backwards.
    ForAllFields(object, /*reverse=*/true, [&](const r::Field *field) {
      const int32_t num_padding_bytes = field->padding();
      if (num_padding_bytes) {
        code += "  builder:Pad(" + NumToString(num_padding_bytes) + ")\n";
      }
      if (IsStructOrTable(field->type()->base_type())) {
        const r::Object *field_object = GetObject(field->type());
        code += AppendStructBuilderBody(field_object,
                                        prefix + namer_.Variable(*field) + "_");
      } else {
        code += "  builder:Prepend" + GenerateMethod(field) + "(" + prefix +
                namer_.Variable(*field) + ")\n";
      }
    });

    return code;
  }

  std::string GenerateMethod(const r::Field *field) const {
    const r::BaseType base_type = field->type()->base_type();
    if (IsScalar(base_type)) { return namer_.Type(GenerateType(base_type)); }
    if (IsStructOrTable(base_type)) { return "Struct"; }
    return "UOffsetTRelative";
  }

  std::string GenerateGetter(const r::Type *type,
                             bool element_type = false) const {
    switch (element_type ? type->element() : type->base_type()) {
      case r::String: return "self.view:String(";
      case r::Union: return "self.view:Union(";
      case r::Vector: return GenerateGetter(type, true);
      default:
        return "self.view:Get(flatbuffers.N." +
               namer_.Type(GenerateType(type, element_type)) + ", ";
    }
  }

  std::string GenerateType(const r::Type *type,
                           bool element_type = false) const {
    const r::BaseType base_type =
        element_type ? type->element() : type->base_type();
    if (IsScalar(base_type)) { return GenerateType(base_type); }
    switch (base_type) {
      case r::String: return "string";
      case r::Vector: return GenerateGetter(type, true);
      case r::Obj: return namer_.Type(namer_.Denamespace(GetObject(type)));

      default: return "*flatbuffers.Table";
    }
  }

  std::string GenerateType(const r::BaseType base_type) const {
    // Need to override the default naming to match the Ruby runtime libraries.
    // TODO(derekbailey): make overloads in the runtime libraries to avoid this.
    switch (base_type) {
      case r::None: return "uint8";
      case r::UType: return "uint8";
      case r::Byte: return "int8";
      case r::UByte: return "uint8";
      case r::Short: return "int16";
      case r::UShort: return "uint16";
      case r::Int: return "int32";
      case r::UInt: return "uint32";
      case r::Long: return "int64";
      case r::ULong: return "uint64";
      case r::Float: return "Float32";
      case r::Double: return "Float64";
      default: return r::EnumNameBaseType(base_type);
    }
  }

  std::string DefaultValue(const r::Field *field) const {
    const r::BaseType base_type = field->type()->base_type();
    if (IsFloatingPoint(base_type)) {
      const auto default_value = field->default_real();
      if (std::isnan(default_value)) {
        return "Float::NAN";
      } else if (std::isinf(default_value)) {
        if (default_value > 0) {
          return "Float::INFINITY";
        } else {
          return "-Float::INFINITY";
        }
      } else {
        return NumToString(default_value);
      }
    }
    if (IsBool(base_type)) {
      return field->default_integer() ? "true" : "false";
    }
    if (IsScalar(base_type)) { return NumToString((field->default_integer())); }
    return "nil";
  }

  void StartCodeBlock(const reflection::Enum *enum_def) {
    current_enum_ = enum_def;
    current_obj_ = nullptr;
    requires_.clear();
  }

  void StartCodeBlock(const reflection::Object *object) {
    current_obj_ = object;
    current_enum_ = nullptr;
    requires_.clear();
  }

  std::pair<std::optional<std::string>, std::optional<std::string>>
  ResolveClassName(const r::Type *type,
                   const std::vector<std::string>& base_ns_components,
                   bool use_element = false,
                   bool always_use_absolute_class_name = false) {
    std::string name;
    std::string ns;

    const r::BaseType base_type =
        use_element ? type->element() : type->base_type();

    if (base_type == r::None) {
      return {std::nullopt, std::nullopt};
    }

    if (IsStructOrTable(base_type)) {
      const r::Object *object = GetObjectByIndex(type->index());
      name = namer_.Denamespace(object, ns);
      if (object == current_obj_) { return {std::nullopt, namer_.Type(name)}; }
    } else {
      const r::Enum *enum_def = GetEnumByIndex(type->index());
      name = namer_.Denamespace(enum_def, ns);
      if (enum_def == current_enum_) { return {std::nullopt, namer_.Type(name)}; }
    }

    auto ns_components = SplitNamespace(ns);
    auto absolute_ns_components = ns_components;
    for (size_t i = 0; i < base_ns_components.size(); ++i) {
      const auto& base_ns_component = base_ns_components[i];
      if (ns_components.empty() || ns_components[0] != base_ns_component) {
        for (; i < base_ns_components.size(); ++i) {
          ns_components.insert(ns_components.begin(), "..");
        }
        break;
      }
      ns_components.erase(ns_components.begin());
    }
    const auto path = namer_.Directories(ns_components) + namer_.File(name, SkipFile::Extension);

    if (!always_use_absolute_class_name &&
        (ns_components.empty() || ns_components[0] != "..")) {
      // We can use relative hierarchy
      return {path, namer_.NamespacedType(ns_components, name)};
    } else {
      // We need to use absolute hierarchy
      return {path, std::string("::") + namer_.NamespacedType(absolute_ns_components, name)};
    }
  }

  std::string RegisterRequires(const r::Type *type,
                               const std::vector<std::string>& base_ns_components,
                               bool use_element = false) {
    auto [path, class_name] = ResolveClassName(type, base_ns_components, use_element);

    if (path) { RegisterRequires(*path); }
    return *class_name;
  }

  void RegisterRequires(const std::string &path) {
    if (std::find(requires_.cbegin(), requires_.cend(), path) ==
        requires_.cend()) {
      requires_.push_back(path);
    }
  }

  void EmitCodeBlock(const std::string &code_block, const std::string &name,
                     const std::vector<std::string> &ns_components,
                     const std::string &declaring_file) const {
    const std::string root_type = schema_->root_table()->name()->str();
    const std::string root_file =
        schema_->root_table()->declaration_file()->str();

    std::string code = "# Automatically generated by the FlatBuffers compiler, "
      "do not modify.\n";
    code += "#\n";
    code += "# flatc version: " + flatc_version_ + "\n";
    code += "# Declared by:   " + declaring_file + "\n";
    code += "# Rooting type:  " + root_type + " (" + root_file + ")\n";
    code += "\n";

    code += "require \"flatbuffers\"\n";
    if (!requires_.empty()) {
      for (const auto& path : requires_) {
        code += "require_relative \"" + path + "\"\n";
      }
    }
    code += "\n";

    code += code_block;

    const auto path = options_.output_path + namer_.Directories(ns_components);
    EnsureDirExists(path);
    const auto file_name = path + "/" + namer_.File(name);
    SaveFile(file_name.c_str(), code, false);
  }

  std::vector<std::string> requires_;
  CodeGenOptions options_;

  const r::Object *current_obj_;
  const r::Enum *current_enum_;
  const std::string flatc_version_;
  const BfbsNamer namer_;
  std::string indent_;
};
}  // namespace

std::unique_ptr<CodeGenerator> NewRubyBfbsGenerator(
    const std::string &flatc_version) {
  return std::unique_ptr<RubyBfbsGenerator>(new RubyBfbsGenerator(flatc_version));
}

}  // namespace flatbuffers
