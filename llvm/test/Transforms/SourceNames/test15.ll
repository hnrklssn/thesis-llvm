; RUN: opt -load LLVMDiagnosticNameTest%shlibext -diagnostic-names -S < %s 2>&1 >&2 | FileCheck %s

; CHECK:  %arrayidx = getelementptr inbounds %struct.MyStruct*, %struct.MyStruct** %arr, i64 %indvars.iv39, !dbg !45 --> arr[i]
; CHECK:  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 %indvars.iv, i32 1, i32 0, !dbg !56 --> arr[i][j].secondField.inner1
; CHECK:  %arrayidx12 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 0, i32 1, i32 2, i64 %indvars.iv, !dbg !59 --> arr[i]->secondField.flexibleArr[j]
; CHECK:---args section--
; CHECK:print variable arr
; CHECK:print variable n

; ModuleID = '../test.c'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, %struct.MyStruct*, [0 x i8] }

; Function Attrs: norecurse nounwind optsize readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct** nocapture readonly %arr, i32 %n) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.value(metadata %struct.MyStruct** %arr, metadata !28, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i32 %n, metadata !29, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i32 0, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i32 0, metadata !31, metadata !DIExpression()), !dbg !41
  %cmp34 = icmp sgt i32 %n, 0, !dbg !42
  br i1 %cmp34, label %for.cond1.preheader.lr.ph, label %for.cond.cleanup, !dbg !43

for.cond1.preheader.lr.ph:                        ; preds = %entry
  %wide.trip.count41 = zext i32 %n to i64, !dbg !42
  br label %for.body4.lr.ph, !dbg !43

for.body4.lr.ph:                                  ; preds = %for.cond1.preheader.lr.ph, %for.cond.cleanup3
  %indvars.iv39 = phi i64 [ 0, %for.cond1.preheader.lr.ph ], [ %indvars.iv.next40, %for.cond.cleanup3 ]
  %secret_int.037 = phi i32 [ 0, %for.cond1.preheader.lr.ph ], [ %add14, %for.cond.cleanup3 ]
  call void @llvm.dbg.value(metadata i32 %secret_int.037, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i64 %indvars.iv39, metadata !31, metadata !DIExpression()), !dbg !41
  call void @llvm.dbg.value(metadata i32 %secret_int.037, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i32 0, metadata !33, metadata !DIExpression()), !dbg !44
  %arrayidx = getelementptr inbounds %struct.MyStruct*, %struct.MyStruct** %arr, i64 %indvars.iv39, !dbg !45
  %0 = load %struct.MyStruct*, %struct.MyStruct** %arrayidx, align 8, !dbg !45, !tbaa !46
  br label %for.body4, !dbg !50

for.cond.cleanup:                                 ; preds = %for.cond.cleanup3, %entry
  %secret_int.0.lcssa = phi i32 [ 0, %entry ], [ %add14, %for.cond.cleanup3 ], !dbg !51
  call void @llvm.dbg.value(metadata i32 %secret_int.0.lcssa, metadata !30, metadata !DIExpression()), !dbg !40
  ret i32 %secret_int.0.lcssa, !dbg !52

for.cond.cleanup3:                                ; preds = %for.body4
  %indvars.iv.next40 = add nuw nsw i64 %indvars.iv39, 1, !dbg !53
  call void @llvm.dbg.value(metadata i32 %add14, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next40, metadata !31, metadata !DIExpression()), !dbg !41
  %exitcond42 = icmp eq i64 %indvars.iv.next40, %wide.trip.count41, !dbg !42
  br i1 %exitcond42, label %for.cond.cleanup, label %for.body4.lr.ph, !dbg !43, !llvm.loop !54

for.body4:                                        ; preds = %for.body4, %for.body4.lr.ph
  %indvars.iv = phi i64 [ 0, %for.body4.lr.ph ], [ %indvars.iv.next, %for.body4 ]
  %secret_int.133 = phi i32 [ %secret_int.037, %for.body4.lr.ph ], [ %add14, %for.body4 ]
  call void @llvm.dbg.value(metadata i32 %secret_int.133, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !33, metadata !DIExpression()), !dbg !44
  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 %indvars.iv, i32 1, i32 0, !dbg !56
  %1 = load i8*, i8** %inner1, align 8, !dbg !56, !tbaa !46
  call void @llvm.dbg.value(metadata i8* %1, metadata !36, metadata !DIExpression()), !dbg !45
  %2 = load i8, i8* %1, align 1, !dbg !57, !tbaa !58
  %conv = sext i8 %2 to i32, !dbg !57
  %arrayidx12 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 0, i32 1, i32 2, i64 %indvars.iv, !dbg !59
  %3 = load i8, i8* %arrayidx12, align 1, !dbg !59, !tbaa !58
  %conv13 = sext i8 %3 to i32, !dbg !59
  %add = add i32 %secret_int.133, %conv, !dbg !60
  %add14 = add i32 %add, %conv13, !dbg !61
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !62
  call void @llvm.dbg.value(metadata i32 %add14, metadata !30, metadata !DIExpression()), !dbg !40
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !33, metadata !DIExpression()), !dbg !44
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count41, !dbg !63
  br i1 %exitcond, label %for.cond.cleanup3, label %for.body4, !dbg !50, !llvm.loop !64
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind optsize readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 814e07ed461faf6ead13aa0794caa224e4ca219d)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 814e07ed461faf6ead13aa0794caa224e4ca219d)"}
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 14, type: !8, scopeLine: 14, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !27)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64)
!13 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "MyStruct", file: !1, line: 7, size: 192, elements: !14)
!14 = !{!15, !16}
!15 = !DIDerivedType(tag: DW_TAG_member, name: "firstField", scope: !13, file: !1, line: 8, baseType: !10, size: 32)
!16 = !DIDerivedType(tag: DW_TAG_member, name: "secondField", scope: !13, file: !1, line: 9, baseType: !17, size: 128, offset: 64)
!17 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 2, size: 128, elements: !18)
!18 = !{!19, !21, !22}
!19 = !DIDerivedType(tag: DW_TAG_member, name: "inner1", scope: !17, file: !1, line: 3, baseType: !20, size: 64)
!20 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!21 = !DIDerivedType(tag: DW_TAG_member, name: "inner2", scope: !17, file: !1, line: 4, baseType: !12, size: 64, offset: 64)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "flexibleArr", scope: !17, file: !1, line: 5, baseType: !23, offset: 128)
!23 = !DICompositeType(tag: DW_TAG_array_type, baseType: !24, elements: !25)
!24 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!25 = !{!26}
!26 = !DISubrange(count: -1)
!27 = !{!28, !29, !30, !31, !33, !36}
!28 = !DILocalVariable(name: "arr", arg: 1, scope: !7, file: !1, line: 14, type: !11)
!29 = !DILocalVariable(name: "n", arg: 2, scope: !7, file: !1, line: 14, type: !10)
!30 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 15, type: !10)
!31 = !DILocalVariable(name: "i", scope: !32, file: !1, line: 16, type: !10)
!32 = distinct !DILexicalBlock(scope: !7, file: !1, line: 16, column: 3)
!33 = !DILocalVariable(name: "j", scope: !34, file: !1, line: 17, type: !10)
!34 = distinct !DILexicalBlock(scope: !35, file: !1, line: 17, column: 5)
!35 = distinct !DILexicalBlock(scope: !32, file: !1, line: 16, column: 3)
!36 = !DILocalVariable(name: "tmp", scope: !37, file: !1, line: 18, type: !39)
!37 = distinct !DILexicalBlock(scope: !38, file: !1, line: 17, column: 33)
!38 = distinct !DILexicalBlock(scope: !34, file: !1, line: 17, column: 5)
!39 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !24, size: 64)
!40 = !DILocation(line: 0, scope: !7)
!41 = !DILocation(line: 0, scope: !32)
!42 = !DILocation(line: 16, column: 21, scope: !35)
!43 = !DILocation(line: 16, column: 3, scope: !32)
!44 = !DILocation(line: 0, scope: !34)
!45 = !DILocation(line: 0, scope: !37)
!46 = !{!47, !47, i64 0}
!47 = !{!"any pointer", !48, i64 0}
!48 = !{!"omnipotent char", !49, i64 0}
!49 = !{!"Simple C/C++ TBAA"}
!50 = !DILocation(line: 17, column: 5, scope: !34)
!51 = !DILocation(line: 15, column: 6, scope: !7)
!52 = !DILocation(line: 21, column: 2, scope: !7)
!53 = !DILocation(line: 16, column: 27, scope: !35)
!54 = distinct !{!54, !43, !55}
!55 = !DILocation(line: 20, column: 5, scope: !32)
!56 = !DILocation(line: 18, column: 41, scope: !37)
!57 = !DILocation(line: 19, column: 21, scope: !37)
!58 = !{!48, !48, i64 0}
!59 = !DILocation(line: 19, column: 28, scope: !37)
!60 = !DILocation(line: 19, column: 26, scope: !37)
!61 = !DILocation(line: 19, column: 18, scope: !37)
!62 = !DILocation(line: 17, column: 29, scope: !38)
!63 = !DILocation(line: 17, column: 23, scope: !38)
!64 = distinct !{!64, !50, !65}
!65 = !DILocation(line: 20, column: 5, scope: !34)
