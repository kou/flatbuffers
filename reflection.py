#!/usr/bin/env python3

import sys
sys.path.append("python")

import reflection.Schema

with open('reflection/reflection.bfbs', 'rb') as f:
  schema = reflection.Schema.Schema.GetRootAs(f.read())
  print(schema.FileIdent())
  print(schema.FileExt())
  print(schema.RootTable().Name())
  print(schema.ObjectsLength())
  print(schema.Objects(0).Name())
  print(schema.Objects(0).Minalign())
  print(schema.Objects(0).DeclarationFile())
  print(schema.AdvancedFeatures())
  print(schema.ServicesLength())
  print(schema.Enums(0).Name())
  print(schema.Enums(0).DocumentationLength())
  print(schema.Enums(0).AttributesLength())
  print(schema.Enums(0).Attributes(0).Key())
  print(schema.Enums(0).Attributes(0).Value())
  print(schema.Enums(0).IsUnion())
  print(schema.Enums(0).UnderlyingType().BaseType())
  print(schema.FbsFiles(0).IncludedFilenamesLength())
  print(schema.Enums(0).Values(0).AttributesLength())
  print(schema.Objects(0).Fields(0).Type().BaseType())
  print(schema.Objects(0).Fields(0).Id())
  print(schema.Objects(0).Fields(0).Offset())
  print(schema.Objects(0).Fields(0).DefaultInteger())
  print(schema.Objects(0).Fields(0).Padding())
  print(schema.Objects(0).Fields(0).Type().Element())
  print(schema.Objects(0).Fields(0).Type().Index())
  print(schema.Objects(0).Fields(0).Type().FixedLength())
  print(schema.Objects(0).Fields(0).Type().BaseSize())
  print(schema.Objects(0).Fields(0).Type().ElementSize())
