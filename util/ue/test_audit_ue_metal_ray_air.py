#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import unittest
from audit_ue_metal_ray_air import query_calls

class QueryCallTests(unittest.TestCase):
    def test_native_lifecycle_required(self):
        text='\n'.join('%x = call i1 @air.'+n+'.instancing.triangle_data()' for n in
            ('allocate_intersection_query','reset_intersection_query','next_intersection_query'))
        self.assertEqual(len(query_calls(text)),3)
        self.assertEqual(query_calls(text.replace('call i1 @air.next','declare i1 @air.next')),[])

    def test_names_and_declarations_do_not_prove_calls(self):
        self.assertEqual(query_calls('declare i1 @air.next_intersection_query.instancing()\n%struct._intersection_query_t = type opaque\n; Lumen HardwareRayTracing'),[])
        self.assertEqual(query_calls('call i1 @ordinary.air.allocate_intersection_query.instancing()'),[])

    def test_ordinary_compute_is_unclassified(self):
        self.assertEqual(query_calls('%x = call float @air.fast_sqrt.f32(float %y)'),[])

if __name__=='__main__':unittest.main()
