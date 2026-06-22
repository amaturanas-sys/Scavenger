package cl.scavenger.app

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.view.ViewGroup
import android.webkit.WebChromeClient
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient

/**
 * App cliente (WebView) de SCAVENGER.
 *
 * Empaqueta la interfaz web (en assets/www) y se conecta al backend de
 * SCAVENGER que el usuario configura dentro de la app (URL del servidor).
 * El backend debe estar corriendo y accesible desde el teléfono.
 */
class MainActivity : Activity() {

    private lateinit var web: WebView

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        web = WebView(this)
        web.layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )

        web.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true          // localStorage (config del servidor)
            databaseEnabled = true
            mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
            cacheMode = WebSettings.LOAD_DEFAULT
        }
        // Mantiene la navegación dentro del WebView.
        web.webViewClient = WebViewClient()
        web.webChromeClient = WebChromeClient()

        web.loadUrl("file:///android_asset/www/index.html")
        setContentView(web)
    }

    @Suppress("DEPRECATION")
    override fun onBackPressed() {
        if (web.canGoBack()) web.goBack() else super.onBackPressed()
    }
}
