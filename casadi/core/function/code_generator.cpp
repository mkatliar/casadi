/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */



#include "code_generator.hpp"
#include "function_internal.hpp"
#include <iomanip>
#include "casadi/core/runtime/runtime_embedded.hpp"

using namespace std;
namespace casadi {

  CodeGenerator::CodeGenerator(const Dictionary& opts) {
    // Default options
    this->prefix = "";
    this->include = "";
    this->mex = false;
    this->cpp_guards = true;
    this->main = false;
    this->real_t = "double";

    // Read options
    for (Dictionary::const_iterator it=opts.begin(); it!=opts.end(); ++it) {
      if (it->first=="prefix") {
        this->prefix = it->second.toString();
      } else if (it->first=="include") {
        this->include = it->second.toString();
      } else if (it->first=="mex") {
        this->mex = it->second;
      } else if (it->first=="cppguards") {
        this->cpp_guards = it->second;
      } else if (it->first=="main") {
        this->main = it->second;
      } else if (it->first=="real_t") {
        this->real_t = it->second.toString();
      } else {
        casadi_error("Unrecongnized option: " << it->first);
      }
    }

    // Includes needed
    if (this->main) addInclude("stdio.h");
    if (this->mex) addInclude("mex.h");
  }

  void CodeGenerator::addFunction(const Function& f, const std::string& fname) {
    f->generateFunction(*this, fname);
    f->generateMeta(*this, fname);
  }

  void CodeGenerator::generate(const std::string& fname) const {
    std::ofstream cfile;
    cfile.open(fname.c_str());
    generate(cfile);
    cfile.close();
  }

  void CodeGenerator::generate(std::ostream& s) const {
    // Print header
    s << "/* This function was automatically generated by CasADi */" << endl;

    // C linkage
    if (this->cpp_guards) {
      s << "#ifdef __cplusplus" << endl;
      s << "extern \"C\" {" << endl;
      s << "#endif" << endl << endl;
    }

    s << includes_.str();
    s << endl;

    // Space saving macro
    s << "#define d double" << endl << endl;

    s << auxiliaries_.str();

    // Print integer constants
    stringstream name;
    for (int i=0; i<integer_constants_.size(); ++i) {
      name.str(string());
      name << "s" << i;
      printVector(s, name.str(), integer_constants_[i]);
    }

    // Print double constants
    for (int i=0; i<double_constants_.size(); ++i) {
      name.str(string());
      name << "c" << i;
      printVector(s, name.str(), double_constants_[i]);
    }

    s << this->functions.str();

    // Mex gateway
    if (this->mex) {
      s << "void mexFunction(int resc, mxArray *resv[], int argc, const mxArray *argv[]) {" << endl
        << "  mex_eval(resc, resv, argc, argv);" << endl
        << "}" << endl << endl;
    }

    // Generate main
    if (this->main) {
      s << "int main(int argc, char* argv[]) {" << endl
        << "  return main_eval(argc, argv);" << endl
        << "}" << endl << endl;
    }

    s << finalization_.str();

    // C linkage
    if (this->cpp_guards) {
      s << "#ifdef __cplusplus" << endl;
      s << "} /* extern \"C\" */" << endl;
      s << "#endif" << endl;
    }

    // End with new line
    s << endl;
  }

  std::string CodeGenerator::numToString(int n) {
    stringstream ss;
    ss << n;
    return ss.str();
  }

  std::string CodeGenerator::work(int n) {
    if (n<0) {
      return "0";
    } else if (n==0) {
      return "w";
    } else {
      return "w+" + numToString(n);
    }
  }

  std::string CodeGenerator::workelement(int n) {
    casadi_assert(n>=0);
    if (n==0) {
      return "*w";
    } else {
      return "w[+" + numToString(n) + "]";
    }
  }

  void CodeGenerator::assign(std::ostream &s, const std::string& lhs, const std::string& rhs) {
    s << "  " << lhs << " = " << rhs << ";" << endl;
  }

  int CodeGenerator::addDependency(const Function& f) {
    // Get the current number of functions before looking for it
    size_t num_f_before = added_dependencies_.size();

    // Get index of the pattern
    const void* h = static_cast<const void*>(f.get());
    int& ind = added_dependencies_[h];

    // Generate it if it does not exist
    if (added_dependencies_.size() > num_f_before) {
      // Add at the end
      ind = num_f_before;

      // Give it a name
      stringstream name;
      name << "f" << ind;

      // Print to file
      f->generateFunction(*this, name.str());
    }

    return ind;
  }

  void CodeGenerator::printVector(std::ostream &s, const std::string& name, const vector<int>& v) {
    s << "int " << name << "[] = {";
    for (int i=0; i<v.size(); ++i) {
      if (i!=0) s << ", ";
      s << v[i];
    }
    s << "};" << endl;
  }

  void CodeGenerator::printVector(std::ostream &s, const std::string& name,
                                  const vector<double>& v) {
    s << "d " << name << "[] = {";
    for (int i=0; i<v.size(); ++i) {
      if (i!=0) s << ", ";
      s << constant(v[i]);
    }
    s << "};" << endl;
  }

  void CodeGenerator::addInclude(const std::string& new_include, bool relative_path) {
    // Register the new element
    bool added = added_includes_.insert(new_include).second;

    // Quick return if it already exists
    if (!added) return;

    // Print to the header section
    if (relative_path) {
      includes_ << "#include \"" << new_include << "\"" << endl;
    } else {
      includes_ << "#include <" << new_include << ">" << endl;
    }
  }

  int CodeGenerator::addSparsity(const Sparsity& sp) {
    // Get the current number of patterns before looking for it
    size_t num_patterns_before = added_sparsities_.size();

    // Get index of the pattern
    const void* h = static_cast<const void*>(sp.get());
    int& ind = added_sparsities_[h];

    // Generate it if it does not exist
    if (added_sparsities_.size() > num_patterns_before) {

      // Compact version of the sparsity pattern
      std::vector<int> sp_compact = sp.compress();

      // Codegen vector
      ind = getConstant(sp_compact, true);
    }

    return ind;
  }

  std::string CodeGenerator::sparsity(const Sparsity& sp) {
    return "s" + numToString(addSparsity(sp));
  }

  int CodeGenerator::getSparsity(const Sparsity& sp) const {
    const void* h = static_cast<const void*>(sp.get());
    PointerMap::const_iterator it=added_sparsities_.find(h);
    casadi_assert(it!=added_sparsities_.end());
    return it->second;
  }

  size_t CodeGenerator::hash(const std::vector<double>& v) {
    // Calculate a hash value for the vector
    std::size_t seed=0;
    if (!v.empty()) {
      casadi_assert(sizeof(double) % sizeof(size_t)==0);
      const int int_len = v.size()*(sizeof(double)/sizeof(size_t));
      const size_t* int_v = reinterpret_cast<const size_t*>(&v.front());
      for (size_t i=0; i<int_len; ++i) {
        hash_combine(seed, int_v[i]);
      }
    }
    return seed;
  }

  size_t CodeGenerator::hash(const std::vector<int>& v) {
    size_t seed=0;
    hash_combine(seed, v);
    return seed;
  }

  int CodeGenerator::getConstant(const std::vector<double>& v, bool allow_adding) {
    // Hash the vector
    size_t h = hash(v);

    // Try to locate it in already added constants
    pair<multimap<size_t, size_t>::iterator, multimap<size_t, size_t>::iterator> eq =
      added_double_constants_.equal_range(h);
    for (multimap<size_t, size_t>::iterator i=eq.first; i!=eq.second; ++i) {
      if (equal(v, double_constants_[i->second])) return i->second;
    }

    if (allow_adding) {
      // Add to constants
      int ind = double_constants_.size();
      double_constants_.push_back(v);
      added_double_constants_.insert(pair<size_t, size_t>(h, ind));
      return ind;
    } else {
      casadi_error("Constant not found");
      return -1;
    }
  }

  int CodeGenerator::getConstant(const std::vector<int>& v, bool allow_adding) {
    // Hash the vector
    size_t h = hash(v);

    // Try to locate it in already added constants
    pair<multimap<size_t, size_t>::iterator, multimap<size_t, size_t>::iterator> eq =
      added_integer_constants_.equal_range(h);
    for (multimap<size_t, size_t>::iterator i=eq.first; i!=eq.second; ++i) {
      if (equal(v, integer_constants_[i->second])) return i->second;
    }

    if (allow_adding) {
      // Add to constants
      int ind = integer_constants_.size();
      integer_constants_.push_back(v);
      added_integer_constants_.insert(pair<size_t, size_t>(h, ind));
      return ind;
    } else {
      casadi_error("Constant not found");
      return -1;
    }
  }

  int CodeGenerator::getDependency(const Function& f) const {
    const void* h = static_cast<const void*>(f.get());
    PointerMap::const_iterator it=added_dependencies_.find(h);
    casadi_assert(it!=added_dependencies_.end());
    return it->second;
  }

  void CodeGenerator::addAuxiliary(Auxiliary f) {
    // Register the new auxiliary
    bool added = added_auxiliaries_.insert(f).second;

    // Quick return if it already exists
    if (!added) return;

    // Add the appropriate function
    switch (f) {
    case AUX_COPY_N:
      auxiliaries_ << codegen_str_copy_n << endl;
      break;
    case AUX_SWAP:
      auxiliaries_ << codegen_str_swap << endl;
      break;
    case AUX_SCAL:
      auxiliaries_ << codegen_str_scal << endl;
      break;
    case AUX_AXPY:
      auxiliaries_ << codegen_str_axpy << endl;
      break;
    case AUX_DOT:
      auxiliaries_ << codegen_str_dot << endl;
      break;
    case AUX_ASUM:
      auxiliaries_ << codegen_str_asum << endl;
      break;
    case AUX_IAMAX:
      auxiliaries_ << codegen_str_iamax << endl;
      break;
    case AUX_NRM2:
      auxiliaries_ << codegen_str_nrm2 << endl;
      break;
    case AUX_FILL_N:
      auxiliaries_ << codegen_str_fill_n << endl;
      break;
    case AUX_MM_SPARSE:
      auxiliaries_ << codegen_str_mm_sparse << endl;
      break;
    case AUX_SQ:
      auxSq();
      break;
    case AUX_SIGN:
      auxSign();
      break;
    case AUX_PROJECT:
      auxiliaries_ << codegen_str_project << endl;
      break;
    case AUX_TRANS:
      auxiliaries_ << codegen_str_trans << endl;
      break;
    case AUX_TO_MEX:
      auxiliaries_
        << "mxArray* casadi_to_mex(const int* sp, d** x) {" << endl
        << "  int nrow = *sp++, ncol = *sp++, nnz = sp[ncol];" << endl
        << "  mxArray* p = mxCreateSparse(nrow, ncol, nnz, mxREAL);" << endl
        << "  int i;" << endl
        << "  mwIndex* j;" << endl
        << "  for (i=0, j=mxGetJc(p); i<=ncol; ++i) *j++ = *sp++;" << endl
        << "  for (i=0, j=mxGetIr(p); i<nnz; ++i) *j++ = *sp++;" << endl
        << "  if (x) *x = (d*)mxGetData(p);" << endl
        << "  return p;" << endl
        << "}" << endl << endl;
      break;
    case AUX_FROM_MEX:
      addAuxiliary(AUX_FILL_N);
      auxiliaries_
        << "d* casadi_from_mex(const mxArray *p, d* y, const int* sp, d* w) {" << endl
        << "  if (!mxIsDouble(p) || mxGetNumberOfDimensions(p)!=2)" << endl
        << "    mexErrMsgIdAndTxt(\"Casadi:RuntimeError\",\"\\\"casadi_from_mex\\\" failed: "
        << "Not a two-dimensional matrix of double precision.\");" << endl
        << "  int nrow = *sp++, ncol = *sp++, nnz = sp[ncol];" << endl
        << "  const int *colind=sp, *row=sp+ncol+1;" << endl
        << "  size_t p_nrow = mxGetM(p), p_ncol = mxGetN(p);" << endl
        << "  const double* p_data = (const double*)mxGetData(p);" << endl
        << "  bool is_sparse = mxIsSparse(p);" << endl
        << "  mwIndex *Jc = is_sparse ? mxGetJc(p) : 0;" << endl
        << "  mwIndex *Ir = is_sparse ? mxGetIr(p) : 0;" << endl
        << "  if (p_nrow==1 && p_ncol==1) {" << endl
        << "    double v = is_sparse && Jc[1]==0 ? 0 : *p_data;" << endl
        << "    casadi_fill_n(y, nnz, v);" << endl
        << "  } else {" << endl
        << "    bool tr = false;" << endl
        << "    if (nrow!=p_nrow || ncol!=p_ncol) {" << endl
        << "      tr = nrow==p_ncol && ncol==p_nrow && (nrow==1 || ncol==1);" << endl
        << "      if (!tr) mexErrMsgIdAndTxt(\"Casadi:RuntimeError\",\"\\\"casadi_from_mex\\\""
        << " failed: Dimension mismatch.\");" << endl
        << "    }" << endl
        << "    int r,c,k;" << endl
        << "    if (is_sparse) {" << endl
        << "      if (tr) {" << endl
        << "        for (c=0; c<ncol; ++c)" << endl
        << "          for (k=colind[c]; k<colind[c+1]; ++k) w[row[k]+c*nrow]=0;" << endl
        << "        for (c=0; c<p_ncol; ++c)" << endl
        << "          for (k=Jc[c]; k<Jc[c+1]; ++k) w[c+Ir[k]*p_ncol] = p_data[k];" << endl
        << "        for (c=0; c<ncol; ++c)" << endl
        << "          for (k=colind[c]; k<colind[c+1]; ++k) y[k] = w[row[k]+c*nrow];" << endl
        << "      } else {" << endl
        << "        for (c=0; c<ncol; ++c) {" << endl
        << "          for (k=colind[c]; k<colind[c+1]; ++k) w[row[k]]=0;" << endl
        << "          for (k=Jc[c]; k<Jc[c+1]; ++k) w[Ir[k]]=p_data[k];" << endl
        << "          for (k=colind[c]; k<colind[c+1]; ++k) y[k]=w[row[k]];" << endl
        << "        }" << endl
        << "      }" << endl
        << "    } else {" << endl
        << "      for (c=0; c<ncol; ++c) {" << endl
        << "        for (k=colind[c]; k<colind[c+1]; ++k) {" << endl
        << "          y[k] = p_data[row[k]+c*nrow];" << endl
        << "        }" << endl
        << "      }" << endl
        << "    }" << endl
        << "  }" << endl
        << "  return y;" << endl
        << "}" << endl << endl;
      break;
    }
  }

  std::string CodeGenerator::to_mex(const Sparsity& sp, const std::string& data) {
    addInclude("mex.h");
    addAuxiliary(AUX_TO_MEX);
    stringstream s;
    s << "casadi_to_mex(" << sparsity(sp) << ", " << data << ");";
    return s.str();
  }

  std::string CodeGenerator::from_mex(std::string& arg,
                                      const std::string& res, std::size_t res_off,
                                      const Sparsity& sp_res, const std::string& w) {
    // Handle offset with recursion
    if (res_off!=0) return from_mex(arg, res+"+"+numToString(res_off), 0, sp_res, w);

    addInclude("mex.h");
    addAuxiliary(AUX_FROM_MEX);
    stringstream s;
    s << "casadi_from_mex(" << arg
      << ", " << res << ", " << sparsity(sp_res) << ", " << w << ");";
    return s.str();
  }

  void CodeGenerator::auxSq() {
    auxiliaries_ << "d casadi_sq(d x) { return x*x;}" << endl;
    auxiliaries_ << "#define sq(x) casadi_sq(x)" << endl << endl;
  }

  void CodeGenerator::auxSign() {
    auxiliaries_ << "d casadi_sign(d x) { return x<0 ? -1 : x>0 ? 1 : x;}" << endl;
    auxiliaries_ << "#define sign(x) casadi_sign(x)" << endl << endl;
  }

  std::string CodeGenerator::constant(double v) {
    stringstream s;
    if (isnan(v)) {
      s << "NAN";
    } else if (isinf(v)) {
      if (v<0) s << "-";
      s << "INFINITY";
    } else {
      int v_int(v);
      if (v_int==v) {
        // Print integer
        s << v_int << ".";
      } else {
        // Print real
        std::ios_base::fmtflags fmtfl = s.flags(); // get current format flags
        s << std::scientific << std::setprecision(std::numeric_limits<double>::digits10 + 1) << v;
        s.flags(fmtfl); // reset current format flags
      }
    }
    return s.str();
  }

  std::string CodeGenerator::copy_n(const std::string& arg, std::size_t arg_off,
                                    std::size_t n, const std::string& res, std::size_t res_off) {
    stringstream s;
    // inline if scalar
    if (n==1) {
      s << res << "[" << res_off << "] = " << arg << "[" << arg_off << "];";
    } else {
      // Handle offsets with recursion
      if (arg_off!=0) return copy_n(arg+"+"+numToString(arg_off), 0, n, res, res_off);
      if (res_off!=0) return copy_n(arg, arg_off, n, res+"+"+numToString(res_off), 0);

      // Perform operation
      addAuxiliary(AUX_COPY_N);
      s << "casadi_copy_n(" << arg << ", " << n << ", " << res << ");";
    }
    return s.str();
  }

  std::string CodeGenerator::fill_n(const std::string& res, std::size_t res_off,
                                    std::size_t n, const std::string& v) {
    stringstream s;
    // Inline if scalar
    if (n==1) {
      s << res << "[" << res_off << "] = " << v << ";";
    } else {
      // Handle offset with recursion
      if (res_off!=0) return fill_n(res+"+"+numToString(res_off), 0, n, v);

      // Perform operation
      addAuxiliary(AUX_FILL_N);
      s << "casadi_fill_n(" << res << ", " << n << ", " << v << ");";
    }
    return s.str();
  }

  std::string CodeGenerator::casadi_dot(int n, const std::string& x, int inc_x,
                                        const std::string& y, int inc_y) {
    addAuxiliary(AUX_DOT);
    stringstream s;
    s << "casadi_dot(" << n << ", " << x << ", " << inc_x << ", " << y << ", " << inc_y << ")";
    return s.str();
  }
  std::string
  CodeGenerator::project(const std::string& arg, std::size_t arg_off, const Sparsity& sp_arg,
                         const std::string& res, std::size_t res_off, const Sparsity& sp_res,
                         const std::string& w) {
    // If sparsity match, simple copy
    if (sp_arg==sp_res) return copy_n(arg, arg_off, sp_arg.nnz(), res, res_off);

    // Handle offsets with recursion
    if (arg_off!=0) return project(arg+"+"+numToString(arg_off), 0, sp_arg,
                                   res, res_off, sp_res, w);
    if (res_off!=0) return project(arg, arg_off, sp_arg,
                                   res+"+"+numToString(res_off), 0, sp_res, w);

    // Create call
    addAuxiliary(CodeGenerator::AUX_PROJECT);
    stringstream s;
    s << "  casadi_project(" << arg << ", " << sparsity(sp_arg) << ", " << res << ", "
      << sparsity(sp_res) << w << ");";
    return s.str();
  }

  std::string CodeGenerator::printf(const std::string& str, const std::vector<std::string>& arg) {
    stringstream s;
    if (this->mex) {
      addInclude("mex.h");
      s << "mexPrintf";
    } else {
      addInclude("stdio.h");
      s << "printf";
    }
    s << "(\"" << str << "\"";
    for (int i=0; i<arg.size(); ++i) s << ", " << arg[i];
    s << ");";
    return s.str();
  }

  std::string CodeGenerator::printf(const std::string& str, const std::string& arg1) {
    std::vector<std::string> arg;
    arg.push_back(arg1);
    return printf(str, arg);
  }

  std::string CodeGenerator::printf(const std::string& str, const std::string& arg1,
                                    const std::string& arg2) {
    std::vector<std::string> arg;
    arg.push_back(arg1);
    arg.push_back(arg2);
    return printf(str, arg);
  }

  std::string CodeGenerator::printf(const std::string& str, const std::string& arg1,
                                    const std::string& arg2, const std::string& arg3) {
    std::vector<std::string> arg;
    arg.push_back(arg1);
    arg.push_back(arg2);
    arg.push_back(arg3);
    return printf(str, arg);
  }

} // namespace casadi

