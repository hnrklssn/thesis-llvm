; RUN: opt -load LLVMHello.so -source-names < %s 2>&1 >&2 | FileCheck %s 

; CHECK:  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !37 --> s.secondField.inner1
; CHECK:  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !48 --> s.secondField.inner2
; CHECK:  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !51 --> s.secondField.inner2->firstField
; CHECK:  %arrayidx = getelementptr inbounds i32**, i32*** %arr, i64 %indvars.iv31, !dbg !58 --> arr[i]
; CHECK:  %firstField12 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !64 --> s.firstField
; CHECK:  %arrayidx7 = getelementptr inbounds i32*, i32** %3, i64 %indvars.iv, !dbg !70 --> arr[i][j]
; CHECK:  %arrayidx8 = getelementptr inbounds i32, i32* %5, i64 5, !dbg !70 --> arr[i][j][5]
; CHECK:---args section--
; CHECK:print variable s
; CHECK:print variable arr
; CHECK:print variable n

; ModuleID = '<stdin>'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, %struct.MyStruct* }

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct* nocapture readonly byval(%struct.MyStruct) align 8 %s, i32*** nocapture readonly %arr, i32 %n) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.declare(metadata %struct.MyStruct* %s, metadata !26, metadata !DIExpression()), !dbg !35
  call void @llvm.dbg.value(metadata i32*** %arr, metadata !27, metadata !DIExpression()), !dbg !36
  call void @llvm.dbg.value(metadata i32 %n, metadata !28, metadata !DIExpression()), !dbg !36
  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !37
  %0 = load i8*, i8** %inner1, align 8, !dbg !37, !tbaa !39
  %tobool = icmp eq i8* %0, null, !dbg !46
  br i1 %tobool, label %if.end, label %if.then, !dbg !47

if.then:                                          ; preds = %entry
  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !48
  %1 = load %struct.MyStruct*, %struct.MyStruct** %inner2, align 8, !dbg !48, !tbaa !50
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !51
  %2 = load i32, i32* %firstField, align 8, !dbg !51, !tbaa !52
  call void @llvm.dbg.value(metadata i32 %2, metadata !29, metadata !DIExpression()), !dbg !36
  br label %if.end, !dbg !53

if.end:                                           ; preds = %if.then, %entry
  %secret_int.0 = phi i32 [ %2, %if.then ], [ 0, %entry ], !dbg !54
  call void @llvm.dbg.value(metadata i32 %secret_int.0, metadata !29, metadata !DIExpression()), !dbg !36
  call void @llvm.dbg.value(metadata i32 0, metadata !30, metadata !DIExpression()), !dbg !55
  %cmp26 = icmp sgt i32 %n, 0, !dbg !56
  br i1 %cmp26, label %for.cond2.preheader.lr.ph, label %for.cond.cleanup, !dbg !57

for.cond2.preheader.lr.ph:                        ; preds = %if.end
  %wide.trip.count33 = zext i32 %n to i64, !dbg !56
  %wide.trip.count = zext i32 %n to i64, !dbg !58
  br label %for.body5.lr.ph, !dbg !57

for.body5.lr.ph:                                  ; preds = %for.cond.cleanup4, %for.cond2.preheader.lr.ph
  %indvars.iv31 = phi i64 [ 0, %for.cond2.preheader.lr.ph ], [ %indvars.iv.next32, %for.cond.cleanup4 ]
  %secret_int.127 = phi i32 [ %secret_int.0, %for.cond2.preheader.lr.ph ], [ %add, %for.cond.cleanup4 ]
  call void @llvm.dbg.value(metadata i64 %indvars.iv31, metadata !30, metadata !DIExpression()), !dbg !55
  call void @llvm.dbg.value(metadata i32 %secret_int.127, metadata !29, metadata !DIExpression()), !dbg !36
  call void @llvm.dbg.value(metadata i32 0, metadata !32, metadata !DIExpression()), !dbg !60
  call void @llvm.dbg.value(metadata i32 %secret_int.127, metadata !29, metadata !DIExpression()), !dbg !36
  %arrayidx = getelementptr inbounds i32**, i32*** %arr, i64 %indvars.iv31, !dbg !58
  %3 = load i32**, i32*** %arrayidx, align 8, !dbg !58, !tbaa !61
  br label %for.body5, !dbg !62

for.cond.cleanup:                                 ; preds = %for.cond.cleanup4, %if.end
  %secret_int.1.lcssa = phi i32 [ %secret_int.0, %if.end ], [ %add, %for.cond.cleanup4 ], !dbg !63
  call void @llvm.dbg.value(metadata i32 %secret_int.1.lcssa, metadata !29, metadata !DIExpression()), !dbg !36
  %firstField12 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !64
  %4 = load i32, i32* %firstField12, align 8, !dbg !64, !tbaa !52
  %add13 = add nsw i32 %4, %secret_int.1.lcssa, !dbg !65
  ret i32 %add13, !dbg !66

for.cond.cleanup4:                                ; preds = %for.body5
  %indvars.iv.next32 = add nuw nsw i64 %indvars.iv31, 1, !dbg !67
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next32, metadata !30, metadata !DIExpression()), !dbg !55
  call void @llvm.dbg.value(metadata i32 %add, metadata !29, metadata !DIExpression()), !dbg !36
  %exitcond34 = icmp eq i64 %indvars.iv.next32, %wide.trip.count33, !dbg !56
  br i1 %exitcond34, label %for.cond.cleanup, label %for.body5.lr.ph, !dbg !57, !llvm.loop !68

for.body5:                                        ; preds = %for.body5, %for.body5.lr.ph
  %indvars.iv = phi i64 [ 0, %for.body5.lr.ph ], [ %indvars.iv.next, %for.body5 ]
  %secret_int.224 = phi i32 [ %secret_int.127, %for.body5.lr.ph ], [ %add, %for.body5 ]
  call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !32, metadata !DIExpression()), !dbg !60
  call void @llvm.dbg.value(metadata i32 %secret_int.224, metadata !29, metadata !DIExpression()), !dbg !36
  %arrayidx7 = getelementptr inbounds i32*, i32** %3, i64 %indvars.iv, !dbg !70
  %5 = load i32*, i32** %arrayidx7, align 8, !dbg !70, !tbaa !61
  %arrayidx8 = getelementptr inbounds i32, i32* %5, i64 5, !dbg !70
  %6 = load i32, i32* %arrayidx8, align 4, !dbg !70, !tbaa !71
  %add = add nsw i32 %6, %secret_int.224, !dbg !72
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !73
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !32, metadata !DIExpression()), !dbg !60
  call void @llvm.dbg.value(metadata i32 %add, metadata !29, metadata !DIExpression()), !dbg !36
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count, !dbg !74
  br i1 %exitcond, label %for.cond.cleanup4, label %for.body5, !dbg !62, !llvm.loop !75
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)"}
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 13, type: !8, scopeLine: 13, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !25)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !22, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_typedef, name: "MyStruct", file: !1, line: 11, baseType: !12)
!12 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "MyStruct", file: !1, line: 6, size: 192, elements: !13)
!13 = !{!14, !15}
!14 = !DIDerivedType(tag: DW_TAG_member, name: "firstField", scope: !12, file: !1, line: 7, baseType: !10, size: 32)
!15 = !DIDerivedType(tag: DW_TAG_member, name: "secondField", scope: !12, file: !1, line: 8, baseType: !16, size: 128, offset: 64)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 2, size: 128, elements: !17)
!17 = !{!18, !20}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "inner1", scope: !16, file: !1, line: 3, baseType: !19, size: 64)
!19 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!20 = !DIDerivedType(tag: DW_TAG_member, name: "inner2", scope: !16, file: !1, line: 4, baseType: !21, size: 64, offset: 64)
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !23, size: 64)
!23 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !24, size: 64)
!24 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!25 = !{!26, !27, !28, !29, !30, !32}
!26 = !DILocalVariable(name: "s", arg: 1, scope: !7, file: !1, line: 13, type: !11)
!27 = !DILocalVariable(name: "arr", arg: 2, scope: !7, file: !1, line: 13, type: !22)
!28 = !DILocalVariable(name: "n", arg: 3, scope: !7, file: !1, line: 13, type: !10)
!29 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 14, type: !10)
!30 = !DILocalVariable(name: "i", scope: !31, file: !1, line: 20, type: !10)
!31 = distinct !DILexicalBlock(scope: !7, file: !1, line: 20, column: 3)
!32 = !DILocalVariable(name: "j", scope: !33, file: !1, line: 21, type: !10)
!33 = distinct !DILexicalBlock(scope: !34, file: !1, line: 21, column: 5)
!34 = distinct !DILexicalBlock(scope: !31, file: !1, line: 20, column: 3)
!35 = !DILocation(line: 13, column: 21, scope: !7)
!36 = !DILocation(line: 0, scope: !7)
!37 = !DILocation(line: 15, column: 19, scope: !38)
!38 = distinct !DILexicalBlock(scope: !7, file: !1, line: 15, column: 5)
!39 = !{!40, !45, i64 8}
!40 = !{!"MyStruct", !41, i64 0, !44, i64 8}
!41 = !{!"int", !42, i64 0}
!42 = !{!"omnipotent char", !43, i64 0}
!43 = !{!"Simple C/C++ TBAA"}
!44 = !{!"Inner", !45, i64 0, !45, i64 8}
!45 = !{!"any pointer", !42, i64 0}
!46 = !DILocation(line: 15, column: 5, scope: !38)
!47 = !DILocation(line: 15, column: 5, scope: !7)
!48 = !DILocation(line: 16, column: 30, scope: !49)
!49 = distinct !DILexicalBlock(scope: !38, file: !1, line: 15, column: 27)
!50 = !{!40, !45, i64 16}
!51 = !DILocation(line: 16, column: 38, scope: !49)
!52 = !{!40, !41, i64 0}
!53 = !DILocation(line: 17, column: 2, scope: !49)
!54 = !DILocation(line: 0, scope: !38)
!55 = !DILocation(line: 0, scope: !31)
!56 = !DILocation(line: 20, column: 21, scope: !34)
!57 = !DILocation(line: 20, column: 3, scope: !31)
!58 = !DILocation(line: 0, scope: !59)
!59 = distinct !DILexicalBlock(scope: !33, file: !1, line: 21, column: 5)
!60 = !DILocation(line: 0, scope: !33)
!61 = !{!45, !45, i64 0}
!62 = !DILocation(line: 21, column: 5, scope: !33)
!63 = !DILocation(line: 16, column: 14, scope: !49)
!64 = !DILocation(line: 23, column: 11, scope: !7)
!65 = !DILocation(line: 23, column: 22, scope: !7)
!66 = !DILocation(line: 23, column: 2, scope: !7)
!67 = !DILocation(line: 20, column: 27, scope: !34)
!68 = distinct !{!68, !57, !69}
!69 = !DILocation(line: 22, column: 32, scope: !31)
!70 = !DILocation(line: 22, column: 21, scope: !59)
!71 = !{!41, !41, i64 0}
!72 = !DILocation(line: 22, column: 18, scope: !59)
!73 = !DILocation(line: 21, column: 29, scope: !59)
!74 = !DILocation(line: 21, column: 23, scope: !59)
!75 = distinct !{!75, !62, !76}
!76 = !DILocation(line: 22, column: 32, scope: !33)
