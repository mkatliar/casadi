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

#ifndef CASADI_CORE_CASADI_EXPORT_H_
#define CASADI_CORE_CASADI_EXPORT_H_

#ifdef CASADI_STATIC_DEFINE
#define CASADI_EXPORT
#define CASADI_NO_EXPORT
#else // CASADI_STATIC_DEFINE
#ifndef CASADI_EXPORT
#ifdef casadi_EXPORTS
// We are building this library
#ifdef _WIN32
#define CASADI_EXPORT __declspec(dllexport)
#else
#define CASADI_EXPORT __attribute__((visibility("default")))
#endif
#else // casadi_EXPORTS
// We are using this library
#ifdef _WIN32
#define CASADI_EXPORT __declspec(dllimport)
#else
#define CASADI_EXPORT __attribute__((visibility("default")))
#endif
#endif // casadi_EXPORTS
#endif // CASADI_EXPORT

#ifndef CASADI_NO_EXPORT
#ifdef _WIN32
#define CASADI_NO_EXPORT
#else
#define CASADI_NO_EXPORT __attribute__((visibility("hidden")))
#endif
#endif // CASADI_NO_EXPORT
#endif // CASADI_STATIC_DEFINE

#endif  // CASADI_CORE_CASADI_EXPORT_H_
