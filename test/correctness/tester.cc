
// =================================================================================================
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. This
// project loosely follows the Google C++ styleguide and uses a tab-size of two spaces and a max-
// width of 100 characters per line.
//
// Author(s):
//   Cedric Nugteren <www.cedricnugteren.nl>
//
// This file implements the Tester class (see the header for information about the class).
//
// =================================================================================================

#include "correctness/tester.h"

#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <limits>

namespace clblast {
// =================================================================================================

// The layouts and transpose-options to test with (data-type dependent)
template <typename T>
const std::vector<Layout> Tester<T>::kLayouts = {Layout::kRowMajor, Layout::kColMajor};
template <> const std::vector<Transpose> Tester<float>::kTransposes = {Transpose::kNo, Transpose::kYes};
template <> const std::vector<Transpose> Tester<double>::kTransposes = {Transpose::kNo, Transpose::kYes};
template <> const std::vector<Transpose> Tester<float2>::kTransposes = {Transpose::kNo, Transpose::kYes, Transpose::kConjugate};
template <> const std::vector<Transpose> Tester<double2>::kTransposes = {Transpose::kNo, Transpose::kYes, Transpose::kConjugate};

// =================================================================================================

// General constructor for all CLBlast testers. It prints out the test header to stdout and sets-up
// the clBLAS library for reference.
template <typename T>
Tester<T>::Tester(const size_t platform_id, const size_t device_id,
                  const std::string &name, const std::vector<std::string> &options):
    platform_(Platform(platform_id)),
    device_(Device(platform_, kDeviceType, device_id)),
    context_(Context(device_)),
    queue_(CommandQueue(context_, device_)),
    error_log_{},
    num_passed_{0},
    num_skipped_{0},
    num_errors_{0},
    print_count_{0},
    tests_failed_{0},
    tests_passed_{0},
    options_{options} {

  // Prints the header
  fprintf(stdout, "* Running on OpenCL device '%s'.\n", device_.Name().c_str());
  fprintf(stdout, "* Starting tests for the %s'%s'%s routine. Legend:\n",
          kPrintMessage.c_str(), name.c_str(), kPrintEnd.c_str());
  fprintf(stdout, "   %s -> Test produced correct results\n", kSuccessData.c_str());
  fprintf(stdout, "   %s -> Test returned the correct error code\n", kSuccessStatus.c_str());
  fprintf(stdout, "   %s -> Test produced incorrect results\n", kErrorData.c_str());
  fprintf(stdout, "   %s -> Test returned an incorrect error code\n", kErrorStatus.c_str());
  fprintf(stdout, "   %s -> Test not executed: OpenCL-kernel compilation error\n",
          kSkippedCompilation.c_str());
  fprintf(stdout, "   %s -> Test not executed: Unsupported precision\n",
          kUnsupportedPrecision.c_str());

  // Initializes clBLAS
  auto status = clblasSetup();
  if (status != CL_SUCCESS) {
    throw std::runtime_error("clBLAS setup error: "+ToString(static_cast<int>(status)));
  }
}

// Destructor prints the summary of the test cases and cleans-up the clBLAS library
template <typename T>
Tester<T>::~Tester() {
  fprintf(stdout, "* Completed all test-cases for this routine. Results:\n");
  fprintf(stdout, "   %lu test(s) succeeded\n", tests_passed_);
  if (tests_failed_ != 0) {
    fprintf(stdout, "   %s%lu test(s) failed%s\n",
            kPrintError.c_str(), tests_failed_, kPrintEnd.c_str());
  }
  else {
    fprintf(stdout, "   %lu test(s) failed\n", tests_failed_);
  }
  fprintf(stdout, "\n");
  clblasTeardown();
}

// =================================================================================================

// Function called at the start of each test. This prints a header with information about the
// test and re-initializes all test data-structures.
template <typename T>
void Tester<T>::TestStart(const std::string &test_name, const std::string &test_configuration) {

  // Prints the header
  fprintf(stdout, "* Testing %s'%s'%s for %s'%s'%s:\n",
          kPrintMessage.c_str(), test_name.c_str(), kPrintEnd.c_str(),
          kPrintMessage.c_str(), test_configuration.c_str(), kPrintEnd.c_str());
  fprintf(stdout, "   ");

  // Empties the error log and the error/pass counters
  error_log_.clear();
  num_passed_ = 0;
  num_skipped_ = 0;
  num_errors_ = 0;
  print_count_ = 0;
}

// Function called at the end of each test. This prints errors if any occured. It also prints a
// summary of the number of sub-tests passed/failed.
template <typename T>
void Tester<T>::TestEnd() {
  fprintf(stdout, "\n");
  if (error_log_.size() == 0) { tests_passed_++; } else { tests_failed_++; }

  // Prints details of all error occurences for these tests
  for (auto &entry: error_log_) {
    if (entry.error_percentage != kStatusError) {
      fprintf(stdout, "   Error rate %.1lf%%: ", entry.error_percentage);
    }
    else {
      fprintf(stdout, "   Status code %d (expected %d): ", entry.status_found, entry.status_expect);
    }
    for (auto &o: options_) {
      if (o == kArgM)        { fprintf(stdout, "%s=%lu ", kArgM, entry.args.m); }
      if (o == kArgN)        { fprintf(stdout, "%s=%lu ", kArgN, entry.args.n); }
      if (o == kArgK)        { fprintf(stdout, "%s=%lu ", kArgK, entry.args.k); }
      if (o == kArgLayout)   { fprintf(stdout, "%s=%d ", kArgLayout, entry.args.layout);}
      if (o == kArgATransp)  { fprintf(stdout, "%s=%d ", kArgATransp, entry.args.a_transpose);}
      if (o == kArgBTransp)  { fprintf(stdout, "%s=%d ", kArgBTransp, entry.args.b_transpose);}
      if (o == kArgSide)     { fprintf(stdout, "%s=%d ", kArgSide, entry.args.side);}
      if (o == kArgTriangle) { fprintf(stdout, "%s=%d ", kArgTriangle, entry.args.triangle);}
      if (o == kArgXInc)     { fprintf(stdout, "%s=%lu ", kArgXInc, entry.args.x_inc);}
      if (o == kArgYInc)     { fprintf(stdout, "%s=%lu ", kArgYInc, entry.args.y_inc);}
      if (o == kArgXOffset)  { fprintf(stdout, "%s=%lu ", kArgXOffset, entry.args.x_offset);}
      if (o == kArgYOffset)  { fprintf(stdout, "%s=%lu ", kArgYOffset, entry.args.y_offset);}
      if (o == kArgALeadDim) { fprintf(stdout, "%s=%lu ", kArgALeadDim, entry.args.a_ld);}
      if (o == kArgBLeadDim) { fprintf(stdout, "%s=%lu ", kArgBLeadDim, entry.args.b_ld);}
      if (o == kArgCLeadDim) { fprintf(stdout, "%s=%lu ", kArgCLeadDim, entry.args.c_ld);}
      if (o == kArgAOffset)  { fprintf(stdout, "%s=%lu ", kArgAOffset, entry.args.a_offset);}
      if (o == kArgBOffset)  { fprintf(stdout, "%s=%lu ", kArgBOffset, entry.args.b_offset);}
      if (o == kArgCOffset)  { fprintf(stdout, "%s=%lu ", kArgCOffset, entry.args.c_offset);}
    }
    fprintf(stdout, "\n");
  }

  // Prints a test summary
  auto pass_rate = 100*num_passed_ / static_cast<float>(num_passed_ + num_skipped_ + num_errors_);
  fprintf(stdout, "   Pass rate %s%5.1lf%%%s:", kPrintMessage.c_str(), pass_rate, kPrintEnd.c_str());
  fprintf(stdout, " %lu passed /", num_passed_);
  if (num_skipped_ != 0) {
    fprintf(stdout, " %s%lu skipped%s /", kPrintWarning.c_str(), num_skipped_, kPrintEnd.c_str());
  }
  else {
    fprintf(stdout, " %lu skipped /", num_skipped_);
  }
  if (num_errors_ != 0) {
    fprintf(stdout, " %s%lu failed%s\n", kPrintError.c_str(), num_errors_, kPrintEnd.c_str());
  }
  else {
    fprintf(stdout, " %lu failed\n", num_errors_);
  }
}

// =================================================================================================

// Compares two floating point values and returns whether they are within an acceptable error
// margin. This replaces GTest's EXPECT_NEAR().
template <typename T>
bool Tester<T>::TestSimilarity(const T val1, const T val2, const double margin) {
  const auto difference = std::fabs(val1 - val2);

  // Shortcut, handles infinities
  if (val1 == val2) {
    return true;
  }
  // The values are zero or both are extremely close to it relative error is less meaningful
  else if (val1 == 0 || val2 == 0 || difference < std::numeric_limits<T>::min()) {
    return difference < (static_cast<T>(margin) * std::numeric_limits<T>::min());
  }
  // Use relative error
  else {
    return (difference / (std::fabs(val1) + std::fabs(val2))) < static_cast<T>(margin);
  }
}

// Specialisations for complex data-types
template <>
bool Tester<float2>::TestSimilarity(const float2 val1, const float2 val2, const double margin) {
  auto real = Tester<float>::TestSimilarity(val1.real(), val2.real(), margin);
  auto imag = Tester<float>::TestSimilarity(val1.imag(), val2.imag(), margin);
  return (real && imag);
}
template <>
bool Tester<double2>::TestSimilarity(const double2 val1, const double2 val2, const double margin) {
  auto real = Tester<double>::TestSimilarity(val1.real(), val2.real(), margin);
  auto imag = Tester<double>::TestSimilarity(val1.imag(), val2.imag(), margin);
  return (real && imag);
}

// =================================================================================================

// Handles a 'pass' or 'error' depending on whether there are any errors
template <typename T>
void Tester<T>::TestErrorCount(const size_t errors, const size_t size, const Arguments<T> &args) {

  // Finished successfully
  if (errors == 0) {
    PrintTestResult(kSuccessData);
    ReportPass();
  }

  // Error(s) occurred
  else {
    auto percentage = 100*errors / static_cast<float>(size);
    PrintTestResult(kErrorData);
    ReportError({StatusCode::kSuccess, StatusCode::kSuccess, percentage, args});
  }
}

// Compares two status codes for equality. The outcome can be a pass (they are the same), a warning
// (CLBlast reported a compilation error), or an error (they are different).
template <typename T>
void Tester<T>::TestErrorCodes(const StatusCode clblas_status, const StatusCode clblast_status,
                            const Arguments<T> &args) {

  // Finished successfully
  if (clblas_status == clblast_status) {
    PrintTestResult(kSuccessStatus);
    ReportPass();
  }

  // No support for this kind of precision
  else if (clblast_status == StatusCode::kNoDoublePrecision ||
           clblast_status == StatusCode::kNoHalfPrecision) {
    PrintTestResult(kUnsupportedPrecision);
    ReportSkipped();
  }

  // Could not compile the CLBlast kernel properly
  else if (clblast_status == StatusCode::kBuildProgramFailure ||
           clblast_status == StatusCode::kNotImplemented) {
    PrintTestResult(kSkippedCompilation);
    ReportSkipped();
  }

  // Error occurred
  else {
    PrintTestResult(kErrorStatus);
    ReportError({clblas_status, clblast_status, kStatusError, args});
  }
}

// =================================================================================================

// Retrieves a list of example scalar values, used for the alpha and beta arguments for the various
// routines. This function is specialised for the different data-types.
template <>
const std::vector<float> Tester<float>::GetExampleScalars() {
  return {0.0f, 1.0f, 3.14f};
}
template <>
const std::vector<double> Tester<double>::GetExampleScalars() {
  return {0.0, 1.0, 3.14};
}
template <>
const std::vector<float2> Tester<float2>::GetExampleScalars() {
  return {{0.0f, 0.0f}, {1.0f, 1.3f}, {2.42f, 3.14f}};
}
template <>
const std::vector<double2> Tester<double2>::GetExampleScalars() {
  return {{0.0, 0.0}, {1.0, 1.3}, {2.42, 3.14}};
}

// =================================================================================================

// A test can either pass, be skipped, or fail
template <typename T>
void Tester<T>::ReportPass() {
  num_passed_++;
}
template <typename T>
void Tester<T>::ReportSkipped() {
  num_skipped_++;
}
template <typename T>
void Tester<T>::ReportError(const ErrorLogEntry &error_log_entry) {
  error_log_.push_back(error_log_entry);
  num_errors_++;
}

// =================================================================================================

// Prints the test-result symbol to screen. This function limits the maximum number of symbols per
// line by printing newlines once every so many calls.
template <typename T>
void Tester<T>::PrintTestResult(const std::string &message) {
  if (print_count_ == kResultsPerLine) {
    print_count_ = 0;
    fprintf(stdout, "\n   ");
  }
  fprintf(stdout, "%s", message.c_str());
  std::cout << std::flush;
  print_count_++;
}

// =================================================================================================

// Compiles the templated class
template class Tester<float>;
template class Tester<double>;
template class Tester<float2>;
template class Tester<double2>;

// =================================================================================================
} // namespace clblast