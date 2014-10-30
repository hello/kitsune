/* Automatically generated nanopb constant definitions */
/* Generated by nanopb-0.3.1 at Tue Oct 28 16:47:22 2014. */

#include "matrix.pb.h"

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif



const pb_field_t MatrixClientMessage_fields[5] = {
    PB_FIELD(  1, BYTES   , OPTIONAL, CALLBACK, FIRST, MatrixClientMessage, mac, mac, 0),
    PB_FIELD(  2, INT32   , OPTIONAL, STATIC  , OTHER, MatrixClientMessage, unix_time, mac, 0),
    PB_FIELD(  3, MESSAGE , OPTIONAL, STATIC  , OTHER, MatrixClientMessage, matrix_payload, unix_time, &Matrix_fields),
    PB_FIELD(  4, MESSAGE , REPEATED, CALLBACK, OTHER, MatrixClientMessage, matrix_list, matrix_payload, &Matrix_fields),
    PB_LAST_FIELD
};

const pb_field_t Matrix_fields[11] = {
    PB_FIELD(  1, STRING  , REQUIRED, CALLBACK, FIRST, Matrix, id, id, 0),
    PB_FIELD(  2, INT32   , REQUIRED, STATIC  , OTHER, Matrix, rows, id, 0),
    PB_FIELD(  3, INT32   , REQUIRED, STATIC  , OTHER, Matrix, cols, rows, 0),
    PB_FIELD(  4, ENUM    , REQUIRED, STATIC  , OTHER, Matrix, datatype, cols, 0),
    PB_FIELD(  5, SINT32  , REPEATED, CALLBACK, OTHER, Matrix, idata, datatype, 0),
    PB_FIELD(  6, FLOAT   , REPEATED, CALLBACK, OTHER, Matrix, fdata, idata, 0),
    PB_FIELD(  7, INT64   , REQUIRED, STATIC  , OTHER, Matrix, time1, fdata, 0),
    PB_FIELD(  8, INT64   , REQUIRED, STATIC  , OTHER, Matrix, time2, time1, 0),
    PB_FIELD(  9, STRING  , REQUIRED, CALLBACK, OTHER, Matrix, tags, time2, 0),
    PB_FIELD( 10, STRING  , REQUIRED, CALLBACK, OTHER, Matrix, source, tags, 0),
    PB_LAST_FIELD
};


/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_32BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in 8 or 16 bit
 * field descriptors.
 */
PB_STATIC_ASSERT((pb_membersize(MatrixClientMessage, matrix_payload) < 65536 && pb_membersize(MatrixClientMessage, matrix_list) < 65536), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_MatrixClientMessage_Matrix)
#endif

#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_16BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in the default
 * 8 bit descriptors.
 */
PB_STATIC_ASSERT((pb_membersize(MatrixClientMessage, matrix_payload) < 256 && pb_membersize(MatrixClientMessage, matrix_list) < 256), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_MatrixClientMessage_Matrix)
#endif


