; RUN: opt -load LLVMHello.so -source-names < %s 2>&1 >&2 | FileCheck %s 

; CHECK: %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !27 --> s.secondField.inner1
; CHECK:   %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !45 --> s.firstField
; CHECK: ---args section--
; CHECK: print variable s

; ModuleID = '../test.c'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, i8* }

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct* nocapture readonly byval(%struct.MyStruct) align 8 %s) local_unnamed_addr #0 !dbg !10 {
entry:
  call void @llvm.dbg.declare(metadata %struct.MyStruct* %s, metadata !24, metadata !DIExpression()), !dbg !26
  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !27
  %0 = load i8*, i8** %inner1, align 8, !dbg !27, !tbaa !29
  %tobool = icmp eq i8* %0, null, !dbg !36
  br i1 %tobool, label %if.end, label %if.then, !dbg !37

if.then:                                          ; preds = %entry
  %1 = bitcast i8* %0 to i32*, !dbg !38
  %2 = load i32, i32* %1, align 4, !dbg !40, !tbaa !41
  call void @llvm.dbg.value(metadata i32 %2, metadata !25, metadata !DIExpression()), !dbg !42
  br label %if.end, !dbg !43

if.end:                                           ; preds = %entry, %if.then
  %secret_int.0 = phi i32 [ %2, %if.then ], [ 0, %entry ], !dbg !44
  call void @llvm.dbg.value(metadata i32 %secret_int.0, metadata !25, metadata !DIExpression()), !dbg !42
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !45
  %3 = load i32, i32* %firstField, align 8, !dbg !45, !tbaa !46
  %add = add nsw i32 %3, %secret_int.0, !dbg !47
  ret i32 %add, !dbg !48
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!5 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!6 = !{i32 2, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{!"clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)"}
!10 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 12, type: !11, scopeLine: 12, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !23)
!11 = !DISubroutineType(types: !12)
!12 = !{!5, !13}
!13 = !DIDerivedType(tag: DW_TAG_typedef, name: "MyStruct", file: !1, line: 10, baseType: !14)
!14 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "MyStruct", file: !1, line: 5, size: 192, elements: !15)
!15 = !{!16, !17}
!16 = !DIDerivedType(tag: DW_TAG_member, name: "firstField", scope: !14, file: !1, line: 6, baseType: !5, size: 32)
!17 = !DIDerivedType(tag: DW_TAG_member, name: "secondField", scope: !14, file: !1, line: 7, baseType: !18, size: 128, offset: 64)
!18 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 1, size: 128, elements: !19)
!19 = !{!20, !22}
!20 = !DIDerivedType(tag: DW_TAG_member, name: "inner1", scope: !18, file: !1, line: 2, baseType: !21, size: 64)
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "inner2", scope: !18, file: !1, line: 3, baseType: !21, size: 64, offset: 64)
!23 = !{!24, !25}
!24 = !DILocalVariable(name: "s", arg: 1, scope: !10, file: !1, line: 12, type: !13)
!25 = !DILocalVariable(name: "secret_int", scope: !10, file: !1, line: 13, type: !5)
!26 = !DILocation(line: 12, column: 21, scope: !10)
!27 = !DILocation(line: 14, column: 19, scope: !28)
!28 = distinct !DILexicalBlock(scope: !10, file: !1, line: 14, column: 5)
!29 = !{!30, !35, i64 8}
!30 = !{!"MyStruct", !31, i64 0, !34, i64 8}
!31 = !{!"int", !32, i64 0}
!32 = !{!"omnipotent char", !33, i64 0}
!33 = !{!"Simple C/C++ TBAA"}
!34 = !{!"Inner", !35, i64 0, !35, i64 8}
!35 = !{!"any pointer", !32, i64 0}
!36 = !DILocation(line: 14, column: 5, scope: !28)
!37 = !DILocation(line: 14, column: 5, scope: !10)
!38 = !DILocation(line: 15, column: 17, scope: !39)
!39 = distinct !DILexicalBlock(scope: !28, file: !1, line: 14, column: 27)
!40 = !DILocation(line: 15, column: 16, scope: !39)
!41 = !{!31, !31, i64 0}
!42 = !DILocation(line: 0, scope: !10)
!43 = !DILocation(line: 16, column: 2, scope: !39)
!44 = !DILocation(line: 0, scope: !28)
!45 = !DILocation(line: 19, column: 11, scope: !10)
!46 = !{!30, !31, i64 0}
!47 = !DILocation(line: 19, column: 22, scope: !10)
!48 = !DILocation(line: 19, column: 2, scope: !10)
