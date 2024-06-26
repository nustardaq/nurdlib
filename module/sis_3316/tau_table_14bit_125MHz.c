/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2024
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <module/sis_3316/tau_table.h>

struct tau_entry const tau_14bit_125MHz[] = {
	{3, 63, 0.516117},
	{3, 62, 0.524506},
	{3, 61, 0.533170},
	{3, 60, 0.542123},
	{3, 59, 0.551380},
	{3, 58, 0.560956},
	{3, 57, 0.570868},
	{3, 56, 0.581134},
	{3, 55, 0.591773},
	{3, 54, 0.602806},
	{3, 53, 0.614255},
	{3, 52, 0.626145},
	{3, 51, 0.638501},
	{3, 50, 0.651352},
	{3, 49, 0.664727},
	{3, 48, 0.678659},
	{3, 47, 0.693184},
	{3, 46, 0.708340},
	{3, 45, 0.724170},
	{3, 44, 0.740720},
	{3, 43, 0.758039},
	{3, 42, 0.776184},
	{3, 41, 0.795213},
	{3, 40, 0.815193},
	{3, 39, 0.836199},
	{3, 38, 0.858310},
	{3, 37, 0.881616},
	{3, 36, 0.906216},
	{3, 35, 0.932223},
	{3, 34, 0.959759},
	{3, 33, 0.988964},
	{3, 32, 1.019995},
	{3, 31, 1.053027},
	{3, 30, 1.088262},
	{3, 29, 1.125926},
	{3, 28, 1.166281},
	{3, 27, 1.209625},
	{3, 26, 1.256303},
	{3, 25, 1.306716},
	{3, 24, 1.361329},
	{3, 23, 1.420692},
	{3, 22, 1.485451},
	{3, 21, 1.556378},
	{3, 20, 1.634397},
	{3, 19, 1.720628},
	{3, 18, 1.816442},
	{3, 17, 1.923527},
	{3, 16, 2.043997},
	{2, 63, 2.076505},
	{2, 62, 2.110062},
	{2, 61, 2.144719},
	{2, 60, 2.180531},
	{3, 15, 2.180531},
	{2, 59, 2.217557},
	{2, 58, 2.255860},
	{2, 57, 2.295506},
	{2, 56, 2.336569},
	{3, 14, 2.336569},
	{2, 55, 2.379125},
	{2, 54, 2.423257},
	{2, 53, 2.469054},
	{2, 52, 2.516613},
	{3, 13, 2.516613},
	{2, 51, 2.566037},
	{2, 50, 2.617438},
	{2, 49, 2.670937},
	{2, 48, 2.726665},
	{3, 12, 2.726665},
	{2, 47, 2.784764},
	{2, 46, 2.845389},
	{2, 45, 2.908709},
	{2, 44, 2.974907},
	{3, 11, 2.974907},
	{2, 43, 3.044184},
	{2, 42, 3.116760},
	{2, 41, 3.192876},
	{2, 40, 3.272798},
	{3, 10, 3.272798},
	{2, 39, 3.356819},
	{2, 38, 3.445262},
	{2, 37, 3.538485},
	{2, 36, 3.636887},
	{3, 9, 3.636887},
	{2, 35, 3.740913},
	{2, 34, 3.851057},
	{2, 33, 3.967877},
	{2, 32, 4.091999},
	{3, 8, 4.091999},
	{2, 31, 4.224128},
	{2, 30, 4.365065},
	{2, 29, 4.515723},
	{2, 28, 4.677142},
	{3, 7, 4.677142},
	{2, 27, 4.850517},
	{2, 26, 5.037230},
	{2, 25, 5.238879},
	{2, 24, 5.457332},
	{3, 6, 5.457332},
	{2, 23, 5.694782},
	{2, 22, 5.953817},
	{2, 21, 6.237523},
	{2, 20, 6.549599},
	{3, 5, 6.549599},
	{2, 19, 6.894526},
	{2, 18, 7.277777},
	{2, 17, 7.706117},
	{2, 16, 8.187999},
	{3, 4, 8.187999},
	{1, 63, 8.318031},
	{1, 62, 8.452257},
	{1, 61, 8.590885},
	{1, 60, 8.734133},
	{2, 15, 8.734133},
	{1, 59, 8.882237},
	{1, 58, 9.035448},
	{1, 57, 9.194035},
	{1, 56, 9.358285},
	{2, 14, 9.358285},
	{1, 55, 9.528509},
	{1, 54, 9.705036},
	{1, 53, 9.888226},
	{1, 52, 10.078461},
	{2, 13, 10.078461},
	{1, 51, 10.276156},
	{1, 50, 10.481759},
	{1, 49, 10.695755},
	{1, 48, 10.918666},
	{2, 12, 10.918666},
	{3, 3, 10.918666},
	{1, 47, 11.151063},
	{1, 46, 11.393565},
	{1, 45, 11.646844},
	{1, 44, 11.911636},
	{2, 11, 11.911636},
	{1, 43, 12.188744},
	{1, 42, 12.479047},
	{1, 41, 12.783512},
	{1, 40, 13.103200},
	{2, 10, 13.103200},
	{1, 39, 13.439282},
	{1, 38, 13.793052},
	{1, 37, 14.165946},
	{1, 36, 14.559555},
	{2, 9, 14.559555},
	{1, 35, 14.975657},
	{1, 34, 15.416235},
	{1, 33, 15.883515},
	{1, 32, 16.380000},
	{2, 8, 16.380000},
	{3, 2, 16.380000},
	{1, 31, 16.908516},
	{1, 30, 17.472266},
	{1, 29, 18.074896},
	{1, 28, 18.720571},
	{2, 7, 18.720571},
	{1, 27, 19.414074},
	{1, 26, 20.160923},
	{1, 25, 20.967520},
	{1, 24, 21.841333},
	{2, 6, 21.841333},
	{1, 23, 22.791130},
	{1, 22, 23.827273},
	{1, 21, 24.962095},
	{1, 20, 26.210400},
	{2, 5, 26.210400},
	{1, 19, 27.590105},
	{1, 18, 29.123111},
	{1, 17, 30.836470},
	{1, 16, 32.764000},
	{2, 4, 32.764000},
	{3, 1, 32.764000},
	{0, 63, 33.284127},
	{0, 62, 33.821032},
	{0, 61, 34.375541},
	{0, 60, 34.948533},
	{1, 15, 34.948533},
	{0, 59, 35.540949},
	{0, 58, 36.153793},
	{0, 57, 36.788140},
	{0, 56, 37.445143},
	{1, 14, 37.445143},
	{0, 55, 38.126036},
	{0, 54, 38.832148},
	{0, 53, 39.564906},
	{0, 52, 40.325846},
	{1, 13, 40.325846},
	{0, 51, 41.116627},
	{0, 50, 41.939040},
	{0, 49, 42.795020},
	{0, 48, 43.686667},
	{1, 12, 43.686667},
	{2, 3, 43.686667},
	{0, 47, 44.616255},
	{0, 46, 45.586261},
	{0, 45, 46.599378},
	{0, 44, 47.658545},
	{1, 11, 47.658545},
	{0, 43, 48.766977},
	{0, 42, 49.928190},
	{0, 41, 51.146049},
	{0, 40, 52.424800},
	{1, 10, 52.424800},
	{0, 39, 53.769128},
	{0, 38, 55.184210},
	{0, 37, 56.675784},
	{0, 36, 58.250222},
	{1, 9, 58.250222},
	{0, 35, 59.914628},
	{0, 34, 61.676941},
	{0, 33, 63.546061},
	{0, 32, 65.532000},
	{1, 8, 65.532000},
	{2, 2, 65.532000},
	{0, 31, 67.646064},
	{0, 30, 69.901067},
	{0, 29, 72.311586},
	{0, 28, 74.894286},
	{1, 7, 74.894286},
	{0, 27, 77.668296},
	{0, 26, 80.655692},
	{0, 25, 83.882080},
	{0, 24, 87.377333},
	{1, 6, 87.377333},
	{0, 23, 91.176522},
	{0, 22, 95.321091},
	{0, 21, 99.860381},
	{0, 20, 104.853600},
	{1, 5, 104.853600},
	{0, 19, 110.372421},
	{0, 18, 116.504444},
	{0, 17, 123.357882},
	{0, 16, 131.068000},
	{1, 4, 131.068000},
	{2, 1, 131.068000},
	{0, 15, 139.806133},
	{0, 14, 149.792571},
	{0, 13, 161.315385},
	{0, 12, 174.758667},
	{1, 3, 174.758667},
	{0, 11, 190.646182},
	{0, 10, 209.711200},
	{0, 9, 233.012889},
	{0, 8, 262.140000},
	{1, 2, 262.140000},
	{0, 7, 299.589143},
	{0, 6, 349.521333},
	{0, 5, 419.426400},
	{0, 4, 524.284000},
	{1, 1, 524.284000},
	{0, 3, 699.046667},
	{0, 2, 1048.572000},
	{0, 1, 2097.148000},
};
