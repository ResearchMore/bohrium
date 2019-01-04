/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>

#if PY_MAJOR_VERSION >= 3
#define NPY_PY3K
#endif

#define BhAPI_MODULE

#include <bohrium_api.h>  // Notice, `bohrium_api.h` is auto generated by `setup.py`
#include <bhc.h>

/** Notice, all functions starting with `BhAPI_` will be part of the auto generated C-API.
 * See the `write_header()` function in `setup.py`
 */

/// Flush the Bohrium runtime system
static void BhAPI_flush(void) {
    bhc_flush();
}

/// Get the number of times flush has been called
static int BhAPI_flush_count(void) {
    return bhc_flush_count();
}

/// Flush and repeat the lazy evaluated operations `nrepeats` times.
static void BhAPI_flush_and_repeat(uint64_t nrepeats) {
    bhc_flush_and_repeat(nrepeats);
}

/// Flush and repeat the lazy evaluated operations until `condition` is false or `nrepeats` is reached.
static void BhAPI_flush_and_repeat_condition(uint64_t nrepeats, bhc_ndarray_bool8_p condition) {
    bhc_flush_and_repeat_condition(nrepeats, condition);
}

/// Send and receive a message through the component stack
/// NB: the returned string is invalidated on the next call to BhAPI_message()
static const char *BhAPI_message(const char *msg) {
    return bhc_message(msg);
}

/// Get the device context, such as OpenCL's cl_context, of the first VE in the runtime stack.
/// If the first VE isn't a device, NULL is returned.
static void* BhAPI_getDeviceContext(void) {
    return bhc_getDeviceContext();
}

/// Set the context handle, such as CUDA's context, of the first VE in the runtime stack.
/// If the first VE isn't a device, nothing happens.
static void BhAPI_set_device_context(uint64_t device_context) {
    bhc_set_device_context(device_context);
}

/// Create new flat array
static void *BhAPI_new(bhc_dtype dtype, uint64_t size) {
    return bhc_new(dtype, size);
};

/// Destroy array
static void BhAPI_destroy(bhc_dtype dtype, void *ary) {
    bhc_destroy(dtype, ary);
};

/// Create view of a flat array `src`
static void *BhAPI_view(bhc_dtype dtype, void *src, int64_t rank, int64_t start, const int64_t *shape,
                        const int64_t *stride) {
    return bhc_view(dtype, src, rank, start, shape, stride);
}

/// Informs the runtime system to make data synchronized and available after the next flush().
static void BhAPI_sync(bhc_dtype dtype, const void *ary) {
    bhc_sync(dtype, ary);
}

/// Set a reset for an iterator in a dynamic view within a loop
static void BhAPI_add_reset(bhc_dtype dtype, const void *ary1, size_t dim, size_t reset_max) {
    bhc_add_reset(dtype, ary1, dim, reset_max);
}

/// Do array operation based on `opcode`
static void BhAPI_op(bhc_opcode opcode, const bhc_dtype types[], const bhc_bool constants[], void *operands[]) {
    bhc_op(opcode, types, constants, operands);
}

/** Fill out with random data.
  The returned result is a deterministic function of the key and counter,
  i.e. a unique (seed, indexes) tuple will always produce the same result.
  The result is highly sensitive to small changes in the inputs, so that the sequence
  of values produced by simply incrementing the counter (or key) is effectively
  indistinguishable from a sequence of samples of a uniformly distributed random variable.

  random123(out, seed, key) where: 'out' is the array to fill with random data
                                   'seed' is the seed of a random sequence
                                   'key' is the index in the random sequence
*/
static void BhAPI_random123(void *out, uint64_t seed, uint64_t key) {
    bhc_random123_Auint64_Kuint64_Kuint64(out, seed, key);
}

/// Extension Method, returns 0 when the extension exist
static int BhAPI_extmethod(bhc_dtype dtype, const char *name, const void *out, const void *in1, const void *in2) {
    return bhc_extmethod(dtype, name, out, in1, in2);
}

/// Get data pointer from the first VE in the runtime stack
///   if 'copy2host', always copy the memory to main memory
///   if 'force_alloc', force memory allocation before returning the data pointer
///   if 'nullify', set the data pointer to NULL after returning the data pointer
static void *BhAPI_data_get(bhc_dtype dtype, const void *ary, bhc_bool copy2host, bhc_bool force_alloc,
                            bhc_bool nullify) {
    return bhc_data_get(dtype, ary, copy2host, force_alloc, nullify);
}

/// Set data pointer in the first VE in the runtime stack
/// NB: The component will deallocate the memory when encountering a BH_FREE
///   if 'host_ptr', the pointer points to the host memory (main memory) as opposed to device memory
static void BhAPI_data_set(bhc_dtype dtype, const void *ary, bhc_bool host_ptr, void *data) {
    bhc_data_set(dtype, ary, host_ptr, data);
}

/// Copy the memory of `src` to `dst`
///   Use 'param' to set compression parameters or use the empty string
static void BhAPI_data_copy(bhc_dtype dtype, const void *src, const void *dst, const char *param) {
    bhc_data_copy(dtype, src, dst, param);
}

/// Slides the view of an array in the given dimensions, by the given strides for each iteration in a loop.
static void BhAPI_slide_view(bhc_dtype dtype, const void *ary1, size_t dim, int slide, int view_shape, int array_shape,
                             int array_stride, int step_delay) {
    bhc_slide_view(dtype, ary1, dim, slide, view_shape, array_shape, array_stride, step_delay);
}

/** Init arrays and signal handler */
static void BhAPI_mem_signal_init(void) {
    bh_mem_signal_init();
}

/** Shutdown of this library */
static void BhAPI_mem_signal_shutdown(void) {
    bh_mem_signal_shutdown();
}

/** Attach continues memory segment to signal handler
 *
 * @param idx - Id to identify the memory segment when executing the callback function.
 * @param addr - Start address of memory segment.
 * @param size - Size of memory segment in bytes
 * @param callback - Callback function which is executed when segfault hits in the memory
 *                   segment. The function is called with the address pointer and the memory segment idx.
 *                   NB: the function must return non-zero on success
 */
static void BhAPI_mem_signal_attach(void *idx, void *addr, uint64_t size,
                                    int (*callback) (void* fault_address, void* segment_idx)) {
    bh_mem_signal_attach(idx, addr, size, callback);
}

/** Detach signal
 *
 * @param addr - Start address of memory segment.
 */
static void BhAPI_mem_signal_detach(const void *addr) {
    bh_mem_signal_detach(addr);
}

/** Check if signal exist
 *
 * @param addr - Start address of memory segment.
 */
static int BhAPI_mem_signal_exist(const void *addr) {
    return bh_mem_signal_exist(addr);
}

/** Run an user kernel
 *
 * @param kernel The source code of the kernel
 * @param nop Number of operands
 * @param operands The operands given to the kernel all of which must be regular arrays
 * @param compile_cmd The compilation command
 * @param tag String that define which component that should handle this kernel (e.g. "openmp", "opencl", or "cuda")
 * @param param Backend specific parameters (e.g. OpenCL needs `global_work_size` and `local_work_size`)
 * @return The compiler output (both stdout and stderr) when the compilation fails else it is the empty string
 */
static const char* BhAPI_user_kernel(const char* kernel, int nop, void *operands[],
                                     const char* compile_cmd, const char* tag, const char* param) {
    return bhc_user_kernel(kernel, nop, operands, compile_cmd, tag, param);
}

PyObject *PyFlush(PyObject *self, PyObject *args) {
    BhAPI_flush();
    Py_RETURN_NONE;
}

PyObject *PyMessage(PyObject *self, PyObject *args, PyObject *kwds) {
    char *msg;
    static char *kwlist[] = {"msg:str", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &msg)) {
        return NULL;
    }
#if defined(NPY_PY3K)
    return PyUnicode_FromString(bhc_message(msg));
#else
    return PyString_FromString(bhc_message(msg));
#endif
}

PyObject *PySanityCheck(PyObject *self, PyObject *args) {
    bhc_ndarray_uint64_p a = bhc_new_Auint64(100);
    bhc_ndarray_uint64_p b = bhc_new_Auint64(1);
    bhc_range_Auint64(a);
    bhc_add_reduce_Auint64_Auint64_Kint64(b, a, 0);
    BhAPI_flush();
    uint64_t *b_data = (uint64_t*) bhc_data_get_Auint64(b, 1, 1, 0);
    int success = (4950 == *b_data);  // The sum of 0..100 is 4950.
    bhc_destroy_Auint64(a);
    bhc_destroy_Auint64(b);
    BhAPI_flush();
    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// The methods (functions) of this module
static PyMethodDef _bh_apiMethods[] = {
        {"flush",   PyFlush,                 METH_NOARGS, "Evaluate all delayed array operations"},
        {"sanity_check", PySanityCheck,      METH_NOARGS, "Check the Bohrium API installation (returns a boolean)."},
        {"message", (PyCFunction) PyMessage, METH_VARARGS | METH_KEYWORDS,
                                                          "Send and receive a message through the Bohrium stack\n"},
        {NULL,      NULL,                    0,           NULL}        /* Sentinel */
};

#if defined(NPY_PY3K)
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_bh_api",/* name of module */
    NULL, /* module documentation, may be NULL */
    -1,   /* size of per-interpreter state of the module or -1 if the module keeps state in global variables. */
    _bh_apiMethods /* the methods of this module */
};
#endif

#if defined(NPY_PY3K)
#define RETVAL m
PyMODINIT_FUNC PyInit__bh_api(void)
#else
#define RETVAL

PyMODINIT_FUNC init_bh_api(void)
#endif
{
    static void *PyBhAPI[BhAPI_num_of_pointers];
    PyObject *c_api_object;
    PyObject *m;
#if defined(NPY_PY3K)
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule("_bh_api", _bh_apiMethods);
#endif
    if (m == NULL) {
        return RETVAL;
    }

    /* Initialize `PyBhAPI` */
    init_c_api_struct(PyBhAPI);

    /* Create a Capsule containing the API pointer array's address */
    c_api_object = PyCapsule_New((void *) PyBhAPI, "bohrium_api._C_API", NULL);
    if (c_api_object != NULL)
        PyModule_AddObject(m, "_C_API", c_api_object);

    return RETVAL;
}
