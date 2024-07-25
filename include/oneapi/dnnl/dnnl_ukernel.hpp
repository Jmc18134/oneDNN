/*******************************************************************************
* Copyright 2024 Intel Corporation
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
*******************************************************************************/

/// @file
/// ukernel C++ API

#ifndef ONEAPI_DNNL_DNNL_UKERNEL_HPP
#define ONEAPI_DNNL_DNNL_UKERNEL_HPP

#include "oneapi/dnnl/dnnl.hpp"
#include "oneapi/dnnl/dnnl_ukernel.h"

/// @addtogroup dnnl_api oneDNN API
/// @{

/// oneDNN namespace
namespace dnnl {

/// @addtogroup dnnl_api_utils
/// @{

/// @cond DO_NOT_DOCUMENT_THIS

template <>
struct handle_traits<dnnl_brgemm_t> {
    static dnnl_status_t destructor(dnnl_brgemm_t p) {
        return dnnl_brgemm_destroy(p);
    }
};

template <>
struct handle_traits<dnnl_transform_t> {
    static dnnl_status_t destructor(dnnl_transform_t p) {
        return dnnl_transform_destroy(p);
    }
};

/// @endcond

/// @} dnnl_api_utils

/// @addtogroup dnnl_api_ukernel Ukernels
/// Collection of ukernels
/// @{

/// ukernel namespace
namespace ukernel {

#ifdef DNNL_EXPERIMENTAL_UKERNEL

/// Packing specification
enum class pack_type {
    /// Undefined pack type. A guard value.
    undef = dnnl_pack_type_undef,
    /// Plain, not transposed layout. Similar to format_tag::ab.
    no_trans = dnnl_pack_type_no_trans,
    /// Plain, transposed layout. Similar to format_tag::ba.
    trans = dnnl_pack_type_trans,
    /// Packed by 32 bits along K dimension layout.
    pack32 = dnnl_pack_type_pack32,
};

/// @addtogroup dnnl_api_ukernel_brgemm BRGeMM ukernel
/// BRGeMM ukernel routines
/// @{

struct brgemm : public handle<dnnl_brgemm_t> {
    /// Default constructor. Produces an empty object.
    brgemm() = default;

    /// Constructs a BRGeMM ukernel object. Operates by the following formula:
    /// `C = [A x B]`.
    ///
    /// @param M Dimension M of tensor A.
    /// @param N Dimension N of tensor B.
    /// @param K Dimension K of tensors A and B.
    /// @param batch_size Number of batches to process.
    /// @param lda Leading dimension of tensor A.
    /// @param ldb Leading dimension of tensor B.
    /// @param ldc Leading dimension of tensor C.
    /// @param a_dt Data type of tensor A.
    /// @param b_dt Data type of tensor B.
    /// @param c_dt Data type of tensor C.
    /// @param allow_empty A flag signifying whether construction is
    ///     allowed to fail without throwing an exception. In this case an
    ///     empty object will be produced. This flag is optional and
    ///     defaults to false.
    brgemm(memory::dim M, memory::dim N, memory::dim K, memory::dim batch_size,
            memory::dim lda, memory::dim ldb, memory::dim ldc,
            memory::data_type a_dt, memory::data_type b_dt,
            memory::data_type c_dt, bool allow_empty = false) {

        dnnl_brgemm_t brgemm = nullptr;
        dnnl_status_t status = dnnl_brgemm_create(&brgemm, M, N, K, batch_size,
                lda, ldb, ldc, memory::convert_to_c(a_dt),
                memory::convert_to_c(b_dt), memory::convert_to_c(c_dt));

        if (!allow_empty)
            error::wrap_c_api(
                    status, "could not create a BRGeMM ukernel object");
        reset(brgemm);
    }

    /// Sets adding an intermediate result to the output tensor C instead of
    /// writing: `C += [A x B]`.
    ///
    /// @param add_C Value to indicate addition. `false` to skip addition, and
    ///     `true` to apply addition.
    void set_add_C(bool add_C) {
        dnnl_status_t status
                = dnnl_brgemm_set_add_C(get(), static_cast<int>(add_C));
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not set add_C attribute");
    }

    /// Sets post-operations to a BRGeMM ukernel object:
    /// `D = post-operations(C)`.
    ///
    /// Post-operations applies if one of the following holds:
    /// * Non-empty attributes are specified.
    /// * Output data type `d_dt` is different from accumulation data type
    ///     `c_dt`.
    ///
    /// @param ldd Leading dimension of tensor D.
    /// @param d_dt Data type of tensor D.
    /// @param attr Primitive attributes to extend the kernel operations.
    void set_post_ops(memory::dim ldd, memory::data_type d_dt,
            const primitive_attr &attr = primitive_attr()) {
        dnnl_status_t status = dnnl_brgemm_set_post_ops(
                get(), ldd, memory::convert_to_c(d_dt), attr.get());
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not set post operations");
    }

    /// Finalizes initialization of a BRGeMM ukernel object.
    ///
    /// This step must be performed prior to querying information from the
    /// object.
    void finalize() {
        dnnl_status_t status = dnnl_brgemm_finalize(get());
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not finalize an object");
    }

    /// Returns the packing type expected by a tensor B of a BRGeMM ukernel
    /// object.
    pack_type get_B_pack_type() const {
        dnnl_pack_type_t c_pack_type;
        dnnl_status_t status = dnnl_brgemm_get_B_pack_type(get(), &c_pack_type);
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not query B pack type");

        return static_cast<pack_type>(c_pack_type);
    }

    /// Returns the size of a scratchpad memory needed for the BRGeMM ukernel
    /// object.
    size_t get_scratchpad_size() const {
        size_t size;
        dnnl_status_t status = dnnl_brgemm_get_scratchpad_size(get(), &size);
        if (status != dnnl_success)
            error::wrap_c_api(status,
                    "could not query a scratchpad size from a BRGeMM ukernel "
                    "object");
        return size;
    }

    /// Initializes the hardware-specific context. Affects the global state for
    /// all BRGeMM ukernel objects. If no initialization required, returns.
    void set_hw_context() const {
        dnnl_status_t status = dnnl_brgemm_set_hw_context(get());
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not set hardware context");
    }

    /// Releases the hardware-specific context. Affects the global state for
    /// all BRGeMM ukernel objects. Must be used after all the execution calls
    /// to BRGeMM ukernel objects.
    static void release_hw_context() {
        dnnl_status_t status = dnnl_brgemm_release_hw_context();
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not release hardware context");
    }

    /// Generates an executable part of BRGeMM ukernel object.
    void generate() {
        dnnl_status_t status = dnnl_brgemm_generate(get());
        if (status != dnnl_success)
            error::wrap_c_api(status, "could not generate a kernel");
    }

    /// Executes a BRGeMM ukernel object.
    ///
    /// @param A Base pointer to a tensor A.
    /// @param B Base pointer to a tensor B.
    /// @param A_B_offsets Vector of pairs of tensors A and B offsets for
    ///     each batch. The number of batches must coincide with the
    ///     `batch_size` value passed at object construction stage.
    /// @param C Pointer to a tensor C (accumulation buffer).
    /// @param scratchpad Pointer to a scratchpad buffer.
    void execute(const void *A, const void *B,
            const std::vector<std::pair<memory::dim, memory::dim>> &A_B_offsets,
            void *C, void *scratchpad) const {
        // TODO: export batch_element to C API later for user to fill it and
        // pass directly to the call.
        dnnl_status_t status = dnnl_brgemm_execute(get(), A, B,
                (const dnnl_dim_t *)A_B_offsets.data(), C, scratchpad);
        if (status != dnnl_success)
            error::wrap_c_api(
                    status, "could not execute a BRGeMM ukernel object");
    }

    /// Executes a BRGeMM ukernel object with post operations.
    ///
    /// @param A Base pointer to a tensor A.
    /// @param B Base pointer to a tensor B.
    /// @param A_B_offsets Vector of pairs of tensors A and B offsets for
    ///     each batch. The number of batches must coincide with the
    ///     `batch_size` value passed at object construction stage.
    /// @param C Pointer to a tensor C (accumulation buffer).
    /// @param D Pointer to a tensor D (output buffer).
    /// @param scratchpad Pointer to a scratchpad buffer.
    /// @param binary_po Binary post-op memory buffer. Must be passed If binary
    ///     post-op was specified at construction call.
    void execute(const void *A, const void *B,
            const std::vector<std::pair<memory::dim, memory::dim>> &A_B_offsets,
            void *C, void *D, void *scratchpad,
            const void *binary_po = nullptr) const {
        // TODO: export batch_element to C API later for user to fill it and
        // pass directly to the call.
        dnnl_status_t status = dnnl_brgemm_execute_postops(get(), A, B,
                (const dnnl_dim_t *)A_B_offsets.data(), C, D, scratchpad,
                binary_po);
        if (status != dnnl_success)
            error::wrap_c_api(
                    status, "could not execute a BRGeMM ukernel object");
    }
};

struct transform : public handle<dnnl_transform_t> {
    /// Default constructor. Produces an empty object.
    transform() = default;

    /// Constructs a transform object.
    ///
    /// @param K Dimension K.
    /// @param N Dimension N.
    /// @param in_ld Input leading dimension.
    /// @param out_ld Output leading dimension. Specifies a block by N dimension
    ///     during data packing.
    /// @param in_dt Input data type.
    /// @param out_dt Output data type.
    /// @param allow_empty A flag signifying whether construction is
    ///     allowed to fail without throwing an exception. In this case an
    ///     empty object will be produced. This flag is optional and
    ///     defaults to false.
    transform(memory::dim K, memory::dim N, memory::dim in_ld,
            memory::dim out_ld, memory::data_type in_dt,
            memory::data_type out_dt, bool allow_empty = false) {

        dnnl_transform_t transform = nullptr;
        dnnl_status_t status = dnnl_transform_create(&transform, K, N, in_ld,
                out_ld, memory::convert_to_c(in_dt),
                memory::convert_to_c(out_dt));

        if (!allow_empty)
            error::wrap_c_api(status,
                    "could not create a BRGeMM ukernel packing B object");
        reset(transform);
    }

    /// Generates an executable part of transform object.
    void generate() {
        dnnl_status_t status = dnnl_transform_generate(get());
        if (status != dnnl_success)
            error::wrap_c_api(status,
                    "could not generate a BRGeMM ukernel packing B object");
    }

    /// Executes a transform object.
    ///
    /// @param in Pointer to an input buffer.
    /// @param out Pointer to an output buffer.
    void execute(const void *in, void *out) const {
        dnnl_status_t status = dnnl_transform_execute(get(), in, out);
        if (status != dnnl_success)
            error::wrap_c_api(status,
                    "could not execute a BRGeMM ukernel packing B object");
    }
};

/// @} dnnl_api_ukernel_brgemm

#endif

} // namespace ukernel

/// @} dnnl_api_ukernel

} // namespace dnnl

/// @} dnnl_api

#endif /* ONEAPI_DNNL_DNNL_UKERNEL_HPP */
