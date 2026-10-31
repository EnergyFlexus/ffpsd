#include "common.hpp"

#include <cstring>
#include <string>
#include <system_error>
#include <typeinfo>

namespace ffpsd_python
{
    namespace
    {
        // ffpsd._errors.<name>, imported on the first error: stubgen loads this module without its package.
        PyObject* ErrorType(const char* name)
        {
            static PyObject* module = nb::module_::import_("ffpsd._errors").release().ptr(); // kept for the life of the process
            return nb::handle(module).attr(name).ptr();
        }

        // A message may hold a legacy layer name in the system code page, so bytes that are not UTF-8 become U+FFFD.
        void Raise(const char* type, const char* message)
        {
            try
            {
                PyObject* error = ErrorType(type);
                PyObject* text = PyUnicode_DecodeUTF8(message, static_cast<Py_ssize_t>(std::strlen(message)), "replace");
                PyErr_SetObject(error, text);
                Py_XDECREF(text);
            }
            catch (nb::python_error& e)
            {
                e.restore();
            }
        }

        void Translate(const std::exception_ptr& exception, void*)
        {
            try
            {
                std::rethrow_exception(exception);
            }
            // nanobind's own, and bad_alloc, which it turns into MemoryError.
            catch (const nb::python_error&)
            {
                throw;
            }
            catch (const nb::builtin_exception&)
            {
                throw;
            }
            catch (const std::bad_cast&)
            {
                throw;
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            // The order of c_api.cpp: the derived classes before their bases.
            catch (const Unsupported& e)
            {
                Raise("UnsupportedError", e.what());
            }
            catch (const std::invalid_argument& e)
            {
                Raise("InvalidArgumentError", e.what());
            }
            catch (const std::out_of_range& e)
            {
                Raise("OutOfRangeError", e.what());
            }
            catch (const std::length_error& e)
            {
                Raise("InvalidArgumentError", e.what());
            }
            catch (const std::logic_error& e)
            {
                Raise("InvalidOperationError", e.what());
            }
            catch (const std::system_error& e)
            {
                Raise("IoError", e.what());
            }
            catch (const std::runtime_error& e)
            {
                Raise("InvalidFileError", e.what());
            }
            catch (const std::exception& e)
            {
                Raise("Error", e.what());
            }
        }
    } // namespace
} // namespace ffpsd_python

NB_MODULE(_ffpsd, m)
{
    using namespace ffpsd_python;

    nb::register_exception_translator(Translate);
    m.def("version", &ffpsd::Version);

    BindTypes(m);
    BindDocument(m);
    BindLayer(m);
    BindFormats(m);
}
