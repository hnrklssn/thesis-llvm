; RUN: opt < %s -gvn -o /dev/null  -pass-remarks-output=%t -S
; RUN: cat %t | FileCheck %s

; CHECK:      --- !Missed
; CHECK-NEXT: Pass:            gvn
; CHECK-NEXT: Name:            LoadClobbered
; CHECK-NEXT: Function:        multipleUsers1
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
; CHECK-NEXT: Function:        multipleUsers2
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:          'load of type '
; CHECK-NEXT:   - Type:            i32
; CHECK-NEXT:   - String:          ' not eliminated'
; CHECK-NEXT:   - String:          ' in favor of '
; CHECK-NEXT:   - OtherAccess:     load
; CHECK-NEXT:   - String:          ' because it is clobbered by '
; CHECK-NEXT:   - ClobberedBy:     call
; CHECK-NEXT: ...
; CHECK:      --- !Missed
; CHECK-NEXT: Pass:            gvn
; CHECK-NEXT: Name:            LoadClobbered
; CHECK-NEXT: Function:        multipleUsers2
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:          'load of type '
; CHECK-NEXT:   - Type:            i32
; CHECK-NEXT:   - String:          ' not eliminated'
; CHECK-NEXT:   - String:          ' in favor of '
; CHECK-NEXT:   - OtherAccess:     load
; CHECK-NEXT:   - String:          ' because it is clobbered by '
; CHECK-NEXT:   - ClobberedBy:     call
; CHECK-NEXT: ...
; CHECK:      --- !Missed
; CHECK-NEXT: Pass:            gvn
; CHECK-NEXT: Name:            LoadClobbered
; CHECK-NEXT: Function:        multipleUsers3
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:          'load of type '
; CHECK-NEXT:   - Type:            i32
; CHECK-NEXT:   - String:          ' not eliminated'
; CHECK-NEXT:   - String:          ' because it is clobbered by '
; CHECK-NEXT:   - ClobberedBy:     call
; CHECK-NEXT: ...


; ModuleID = 'bugpoint-reduced-simplified.bc'
source_filename = "gvn-test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local void @multipleUsers1(i32* %a) local_unnamed_addr #0 {
entry:
  br i1 undef, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  store i32 undef, i32* %a, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  tail call void @clobberingFunc() #1
  %0 = load i32, i32* %a, align 4
  %mul2 = shl nsw i32 %0, 1
  store i32 %mul2, i32* %a, align 4
  ret void
}

declare dso_local void @clobberingFunc() local_unnamed_addr #0

define dso_local void @multipleUsers2(i32* %a) local_unnamed_addr #0 {
entry:
  br i1 undef, label %if.then, label %if.end5

if.then:                                          ; preds = %entry
  %0 = load i32, i32* %a, align 4
  %mul = mul nsw i32 %0, 10
  tail call void @clobberingFunc() #1
  %1 = load i32, i32* %a, align 4
  %mul3 = mul nsw i32 %1, 5
  tail call void @clobberingFunc() #1
  br label %if.end5

if.end5:                                          ; preds = %if.then, %entry
  %2 = load i32, i32* %a, align 4
  %mul9 = shl nsw i32 %2, 1
  store i32 %mul9, i32* %a, align 4
  ret void
}

define dso_local void @multipleUsers3(i32* %a) local_unnamed_addr #0 {
entry:
  br i1 undef, label %if.end5.sink.split, label %if.else

if.else:                                          ; preds = %entry
  store i32 undef, i32* %a, align 4
  br label %if.end5

if.end5.sink.split:                               ; preds = %entry
  store i32 undef, i32* %a, align 4
  br label %if.end5

if.end5:                                          ; preds = %if.end5.sink.split, %if.else
  tail call void @clobberingFunc() #1
  %0 = load i32, i32* %a, align 4
  %mul7 = shl nsw i32 %0, 1
  ret void
}

attributes #0 = { "use-soft-float"="false" }
attributes #1 = { nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 11.0.0 (git@github.com:hnrklssn/thesis-llvm.git 05da7f0ea394d3994834bb5e387a191388691fc2)"}
