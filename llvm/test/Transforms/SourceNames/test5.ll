; RUN: opt -load LLVMDiagnosticNameTest%shlibext -diagnostic-names -S < %s 2>&1 >&2 | FileCheck %s

; CHECK:  %secondField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 1, !dbg !27 --> s.secondField
; CHECK:  %inner1 = getelementptr inbounds %struct.Inner, %struct.Inner* %secondField, i32 0, i32 0, !dbg !29 --> s.secondField.inner1
; CHECK:  %secondField1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 1, !dbg !32 --> s.secondField
; CHECK:  %inner12 = getelementptr inbounds %struct.Inner, %struct.Inner* %secondField1, i32 0, i32 0, !dbg !34 --> s.secondField.inner1
; CHECK:  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 0, !dbg !41 --> s.firstField
; CHECK:---args section--
; CHECK:print variable s

; ModuleID = '../test.c'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, i8* }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @my_fun(%struct.MyStruct* byval(%struct.MyStruct) align 8 %s) #0 !dbg !10 {
entry:
  %secret_int = alloca i32, align 4
  call void @llvm.dbg.declare(metadata %struct.MyStruct* %s, metadata !23, metadata !DIExpression()), !dbg !24
  call void @llvm.dbg.declare(metadata i32* %secret_int, metadata !25, metadata !DIExpression()), !dbg !26
  %secondField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 1, !dbg !27
  %inner1 = getelementptr inbounds %struct.Inner, %struct.Inner* %secondField, i32 0, i32 0, !dbg !29
  %0 = load i8*, i8** %inner1, align 8, !dbg !29
  %tobool = icmp ne i8* %0, null, !dbg !30
  br i1 %tobool, label %if.then, label %if.else, !dbg !31

if.then:                                          ; preds = %entry
  %secondField1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 1, !dbg !32
  %inner12 = getelementptr inbounds %struct.Inner, %struct.Inner* %secondField1, i32 0, i32 0, !dbg !34
  %1 = load i8*, i8** %inner12, align 8, !dbg !34
  %2 = bitcast i8* %1 to i32*, !dbg !35
  %3 = load i32, i32* %2, align 4, !dbg !36
  store i32 %3, i32* %secret_int, align 4, !dbg !37
  br label %if.end, !dbg !38

if.else:                                          ; preds = %entry
  store i32 0, i32* %secret_int, align 4, !dbg !39
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i32 0, i32 0, !dbg !41
  %4 = load i32, i32* %firstField, align 8, !dbg !41
  %5 = load i32, i32* %secret_int, align 4, !dbg !42
  %add = add nsw i32 %4, %5, !dbg !43
  ret i32 %add, !dbg !44
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!5 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!6 = !{i32 2, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{!"clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)"}
!10 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 12, type: !11, scopeLine: 12, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
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
!23 = !DILocalVariable(name: "s", arg: 1, scope: !10, file: !1, line: 12, type: !13)
!24 = !DILocation(line: 12, column: 21, scope: !10)
!25 = !DILocalVariable(name: "secret_int", scope: !10, file: !1, line: 13, type: !5)
!26 = !DILocation(line: 13, column: 6, scope: !10)
!27 = !DILocation(line: 14, column: 7, scope: !28)
!28 = distinct !DILexicalBlock(scope: !10, file: !1, line: 14, column: 5)
!29 = !DILocation(line: 14, column: 19, scope: !28)
!30 = !DILocation(line: 14, column: 5, scope: !28)
!31 = !DILocation(line: 14, column: 5, scope: !10)
!32 = !DILocation(line: 15, column: 25, scope: !33)
!33 = distinct !DILexicalBlock(scope: !28, file: !1, line: 14, column: 27)
!34 = !DILocation(line: 15, column: 37, scope: !33)
!35 = !DILocation(line: 15, column: 17, scope: !33)
!36 = !DILocation(line: 15, column: 16, scope: !33)
!37 = !DILocation(line: 15, column: 14, scope: !33)
!38 = !DILocation(line: 16, column: 2, scope: !33)
!39 = !DILocation(line: 17, column: 14, scope: !40)
!40 = distinct !DILexicalBlock(scope: !28, file: !1, line: 16, column: 9)
!41 = !DILocation(line: 19, column: 11, scope: !10)
!42 = !DILocation(line: 19, column: 24, scope: !10)
!43 = !DILocation(line: 19, column: 22, scope: !10)
!44 = !DILocation(line: 19, column: 2, scope: !10)
