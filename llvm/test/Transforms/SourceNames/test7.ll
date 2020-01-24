; RUN: opt -load LLVMHello.so -source-names < %s 2>&1 >&2 | FileCheck %s 


; CHECK:  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !26 --> s.secondField.inner1
; CHECK:  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !37 --> s.secondField.inner2
; CHECK:  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !40 --> s.secondField.inner2->firstField
; CHECK:  %firstField2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !45 --> s.firstField
; CHECK:---args section--
; CHECK:print variable s

; ModuleID = '<stdin>'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, %struct.MyStruct* }

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct* nocapture readonly byval(%struct.MyStruct) align 8 %s) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.declare(metadata %struct.MyStruct* %s, metadata !23, metadata !DIExpression()), !dbg !25
  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !26
  %0 = load i8*, i8** %inner1, align 8, !dbg !26, !tbaa !28
  %tobool = icmp eq i8* %0, null, !dbg !35
  br i1 %tobool, label %if.end, label %if.then, !dbg !36

if.then:                                          ; preds = %entry
  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !37
  %1 = load %struct.MyStruct*, %struct.MyStruct** %inner2, align 8, !dbg !37, !tbaa !39
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !40
  %2 = load i32, i32* %firstField, align 8, !dbg !40, !tbaa !41
  call void @llvm.dbg.value(metadata i32 %2, metadata !24, metadata !DIExpression()), !dbg !42
  br label %if.end, !dbg !43

if.end:                                           ; preds = %if.then, %entry
  %secret_int.0 = phi i32 [ %2, %if.then ], [ 0, %entry ], !dbg !44
  call void @llvm.dbg.value(metadata i32 %secret_int.0, metadata !24, metadata !DIExpression()), !dbg !42
  %firstField2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !45
  %3 = load i32, i32* %firstField2, align 8, !dbg !45, !tbaa !41
  %add = add nsw i32 %3, %secret_int.0, !dbg !46
  ret i32 %add, !dbg !47
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
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 13, type: !8, scopeLine: 13, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !22)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11}
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
!22 = !{!23, !24}
!23 = !DILocalVariable(name: "s", arg: 1, scope: !7, file: !1, line: 13, type: !11)
!24 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 14, type: !10)
!25 = !DILocation(line: 13, column: 21, scope: !7)
!26 = !DILocation(line: 15, column: 19, scope: !27)
!27 = distinct !DILexicalBlock(scope: !7, file: !1, line: 15, column: 5)
!28 = !{!29, !34, i64 8}
!29 = !{!"MyStruct", !30, i64 0, !33, i64 8}
!30 = !{!"int", !31, i64 0}
!31 = !{!"omnipotent char", !32, i64 0}
!32 = !{!"Simple C/C++ TBAA"}
!33 = !{!"Inner", !34, i64 0, !34, i64 8}
!34 = !{!"any pointer", !31, i64 0}
!35 = !DILocation(line: 15, column: 5, scope: !27)
!36 = !DILocation(line: 15, column: 5, scope: !7)
!37 = !DILocation(line: 16, column: 30, scope: !38)
!38 = distinct !DILexicalBlock(scope: !27, file: !1, line: 15, column: 27)
!39 = !{!29, !34, i64 16}
!40 = !DILocation(line: 16, column: 38, scope: !38)
!41 = !{!29, !30, i64 0}
!42 = !DILocation(line: 0, scope: !7)
!43 = !DILocation(line: 17, column: 2, scope: !38)
!44 = !DILocation(line: 0, scope: !27)
!45 = !DILocation(line: 20, column: 11, scope: !7)
!46 = !DILocation(line: 20, column: 22, scope: !7)
!47 = !DILocation(line: 20, column: 2, scope: !7)
