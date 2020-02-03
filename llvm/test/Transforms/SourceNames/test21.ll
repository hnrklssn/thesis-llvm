; RUN: opt -load LLVMDiagnosticNameTest%shlibext -diagnostic-names -S < %s 2>&1 >&2 | FileCheck %s

; CHECK:  %arrayidx = getelementptr inbounds i32, i32* %arr, i64 %idxprom, !dbg !26 --> arr[my_global_int]
; CHECK:  %arrayidx2 = getelementptr inbounds i32, i32* %arr, i64 %indvars.iv, !dbg !32 --> arr[i]
; CHECK:---args section--
; CHECK:print variable arr
; CHECK:print variable n

; ModuleID = '../test2.c'
source_filename = "../test2.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@my_global_int = external dso_local local_unnamed_addr global i32, align 4

; Function Attrs: nounwind uwtable
define dso_local i32 @my_fun(i32* nocapture readonly %arr, i32 %n) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.value(metadata i32* %arr, metadata !13, metadata !DIExpression()), !dbg !21
  call void @llvm.dbg.value(metadata i32 %n, metadata !14, metadata !DIExpression()), !dbg !21
  %call = tail call i32 (...) @external_func() #3, !dbg !22
  call void @llvm.dbg.value(metadata i32 %call, metadata !15, metadata !DIExpression()), !dbg !21
  call void @llvm.dbg.value(metadata i32 0, metadata !16, metadata !DIExpression()), !dbg !23
  %cmp11 = icmp sgt i32 %n, 0, !dbg !24
  br i1 %cmp11, label %for.body.lr.ph, label %for.cond.cleanup, !dbg !25

for.body.lr.ph:                                   ; preds = %entry
  %0 = load i32, i32* @my_global_int, align 4, !dbg !26, !tbaa !27
  %idxprom = sext i32 %0 to i64, !dbg !26
  %arrayidx = getelementptr inbounds i32, i32* %arr, i64 %idxprom, !dbg !26
  %1 = load i32, i32* %arrayidx, align 4, !dbg !26, !tbaa !27
  %wide.trip.count = zext i32 %n to i64, !dbg !24
  br label %for.body, !dbg !25

for.cond.cleanup:                                 ; preds = %for.body, %entry
  %secret_int.0.lcssa = phi i32 [ %call, %entry ], [ %add3, %for.body ], !dbg !21
  call void @llvm.dbg.value(metadata i32 %secret_int.0.lcssa, metadata !15, metadata !DIExpression()), !dbg !21
  ret i32 %secret_int.0.lcssa, !dbg !31

for.body:                                         ; preds = %for.body, %for.body.lr.ph
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.body ]
  %secret_int.013 = phi i32 [ %call, %for.body.lr.ph ], [ %add3, %for.body ]
  call void @llvm.dbg.value(metadata i32 %secret_int.013, metadata !15, metadata !DIExpression()), !dbg !21
  call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !16, metadata !DIExpression()), !dbg !23
  call void @llvm.dbg.value(metadata i32 %1, metadata !18, metadata !DIExpression()), !dbg !26
  %arrayidx2 = getelementptr inbounds i32, i32* %arr, i64 %indvars.iv, !dbg !32
  %2 = load i32, i32* %arrayidx2, align 4, !dbg !32, !tbaa !27
  %add = add i32 %1, %secret_int.013, !dbg !33
  %add3 = add i32 %add, %2, !dbg !34
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !35
  call void @llvm.dbg.value(metadata i32 %add3, metadata !15, metadata !DIExpression()), !dbg !21
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !16, metadata !DIExpression()), !dbg !23
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count, !dbg !24
  br i1 %exitcond, label %for.cond.cleanup, label %for.body, !dbg !25, !llvm.loop !36
}

declare dso_local i32 @external_func(...) local_unnamed_addr #1

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #2

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind readnone speculatable willreturn }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 93814cbe72d9c220ebfd411c92c64d3d6af4fd73)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "../test2.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 93814cbe72d9c220ebfd411c92c64d3d6af4fd73)"}
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 4, type: !8, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !12)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!12 = !{!13, !14, !15, !16, !18}
!13 = !DILocalVariable(name: "arr", arg: 1, scope: !7, file: !1, line: 4, type: !11)
!14 = !DILocalVariable(name: "n", arg: 2, scope: !7, file: !1, line: 4, type: !10)
!15 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 5, type: !10)
!16 = !DILocalVariable(name: "i", scope: !17, file: !1, line: 6, type: !10)
!17 = distinct !DILexicalBlock(scope: !7, file: !1, line: 6, column: 3)
!18 = !DILocalVariable(name: "tmp", scope: !19, file: !1, line: 7, type: !10)
!19 = distinct !DILexicalBlock(scope: !20, file: !1, line: 6, column: 31)
!20 = distinct !DILexicalBlock(scope: !17, file: !1, line: 6, column: 3)
!21 = !DILocation(line: 0, scope: !7)
!22 = !DILocation(line: 5, column: 19, scope: !7)
!23 = !DILocation(line: 0, scope: !17)
!24 = !DILocation(line: 6, column: 21, scope: !20)
!25 = !DILocation(line: 6, column: 3, scope: !17)
!26 = !DILocation(line: 0, scope: !19)
!27 = !{!28, !28, i64 0}
!28 = !{!"int", !29, i64 0}
!29 = !{!"omnipotent char", !30, i64 0}
!30 = !{!"Simple C/C++ TBAA"}
!31 = !DILocation(line: 10, column: 3, scope: !7)
!32 = !DILocation(line: 8, column: 25, scope: !19)
!33 = !DILocation(line: 8, column: 23, scope: !19)
!34 = !DILocation(line: 8, column: 16, scope: !19)
!35 = !DILocation(line: 6, column: 27, scope: !20)
!36 = distinct !{!36, !25, !37}
!37 = !DILocation(line: 9, column: 3, scope: !17)
