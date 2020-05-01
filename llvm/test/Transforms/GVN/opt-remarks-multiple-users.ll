; RUN: opt < %s -gvn -o /dev/null  -pass-remarks-output=%t -S
; RUN: cat %t | FileCheck %s

; CHECK:      --- !Missed
; CHECK-NEXT: Pass:            gvn
; CHECK-NEXT: Name:            LoadClobbered
; CHECK-NEXT: Function:        multipleUsers
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:          'load of type '
; CHECK-NEXT:   - Type:            i32
; CHECK-NEXT:   - String:          ' not eliminated'
; CHECK-NEXT:   - String:          ' in favor of '
; CHECK-NEXT:   - OtherAccess:     store
; CHECK-NEXT:   - String:          ' because it is clobbered by '
; CHECK-NEXT:   - ClobberedBy:     call
; CHECK-NEXT: ...
; CHECK:      --- !Missed
; CHECK-NEXT: Pass:            gvn
; CHECK-NEXT: Name:            LoadClobbered
; CHECK-NEXT: Function:        multipleUsers
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:          'load of type '
; CHECK-NEXT:   - Type:            i32
; CHECK-NEXT:   - String:          ' not eliminated'
; CHECK-NEXT:   - String:          ' in favor of '
; CHECK-NEXT:   - OtherAccess:     load
; CHECK-NEXT:   - String:          ' because it is clobbered by '
; CHECK-NEXT:   - ClobberedBy:     call
; CHECK-NEXT: ...

; ModuleID = 'bugpoint-reduced-simplified.bc'
source_filename = "gvn-test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local void @multipleUsers(i32* %a, i32 %b) local_unnamed_addr #0 {
entry:
  store i32 %b, i32* %a, align 4
  tail call void @clobberingFunc() #1
  %0 = load i32, i32* %a, align 4
  tail call void @clobberingFunc() #1
  %1 = load i32, i32* %a, align 4
  %add2 = add nsw i32 %1, %0
  unreachable
}

declare dso_local void @clobberingFunc() local_unnamed_addr #0

attributes #0 = { "use-soft-float"="false" }
attributes #1 = { nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)"}
