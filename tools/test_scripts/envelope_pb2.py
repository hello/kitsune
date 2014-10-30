# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: envelope.proto

from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import descriptor_pb2
# @@protoc_insertion_point(imports)




DESCRIPTOR = _descriptor.FileDescriptor(
  name='envelope.proto',
  package='',
  serialized_pb='\n\x0e\x65nvelope.proto\"r\n\x08\x65nvelope\x12$\n\x0cmessage_type\x18\x01 \x02(\x0e\x32\x0e.envelope.type\x12\x15\n\rmessage_bytes\x18\x02 \x02(\x0c\")\n\x04type\x12\x11\n\rPERIODIC_DATA\x10\x01\x12\x0e\n\nAUDIO_DATA\x10\x02')



_ENVELOPE_TYPE = _descriptor.EnumDescriptor(
  name='type',
  full_name='envelope.type',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='PERIODIC_DATA', index=0, number=1,
      options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='AUDIO_DATA', index=1, number=2,
      options=None,
      type=None),
  ],
  containing_type=None,
  options=None,
  serialized_start=91,
  serialized_end=132,
)


_ENVELOPE = _descriptor.Descriptor(
  name='envelope',
  full_name='envelope',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='message_type', full_name='envelope.message_type', index=0,
      number=1, type=14, cpp_type=8, label=2,
      has_default_value=False, default_value=1,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
    _descriptor.FieldDescriptor(
      name='message_bytes', full_name='envelope.message_bytes', index=1,
      number=2, type=12, cpp_type=9, label=2,
      has_default_value=False, default_value="",
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      options=None),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
    _ENVELOPE_TYPE,
  ],
  options=None,
  is_extendable=False,
  extension_ranges=[],
  serialized_start=18,
  serialized_end=132,
)

_ENVELOPE.fields_by_name['message_type'].enum_type = _ENVELOPE_TYPE
_ENVELOPE_TYPE.containing_type = _ENVELOPE;
DESCRIPTOR.message_types_by_name['envelope'] = _ENVELOPE

class envelope(_message.Message):
  __metaclass__ = _reflection.GeneratedProtocolMessageType
  DESCRIPTOR = _ENVELOPE

  # @@protoc_insertion_point(class_scope:envelope)


# @@protoc_insertion_point(module_scope)
