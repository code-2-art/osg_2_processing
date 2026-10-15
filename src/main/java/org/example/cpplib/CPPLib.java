package org.example.cpplib;

/**
 * CPPLib - cpplib 库的入口类。
 *
 * <p>这个类对应 Processing 的“库主类”惯例。Sketch 里只需要：</p>
 * <pre>
 * import org.example.cpplib.*;
 * </pre>
 * 然后就可以通过 Compute / GLView 使用 C++ 侧的能力。
 *
 * <p>包名 / 库名如果要改动（例如发布时换成你自己的域名），需要同时修改：</p>
 * <ul>
 *   <li>本包路径 src/main/java/org/example/cpplib/</li>
 *   <li>build.gradle.kts 里的 libName 与 group</li>
 *   <li>release.properties / library.properties</li>
 *   <li>C++ 侧 native/include/org_example_cpplib_NativeCore.h（重新 javac -h 生成）</li>
 * </ul>
 */
public class CPPLib {

    /** 返回库版本与后端信息，例如 "cpplib 1.0.0 backend=CPU+CUDA ..."。 */
    public static String version() {
        return NativeCore.nativeVersionString();
    }
}
