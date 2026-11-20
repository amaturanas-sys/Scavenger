#!/usr/bin/env python3
import urllib.request, os

urls = {
    "img_10_23_55_45.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/nffu141fqih3.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005241Z&X-Amz-Expires=3600&X-Amz-Signature=f2791dfd6b8a32eba51e9b1fe9ed3744b769b72a30b93f3f85fa69b3d2181755&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_11_23_56_54.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/tzm4cfyh1org.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005242Z&X-Amz-Expires=3600&X-Amz-Signature=f2aed2073ec0fab8d91f04c976a4915870e70293c75c22edd75427a736257ad1&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_12_10_21_55.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/hr62ctw39rva.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005241Z&X-Amz-Expires=3600&X-Amz-Signature=e4148bb219ff9be79a2916ff8d3938a62988c819abfc12a11d3bf31d9dc1ffa4&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_13_10_23_11.jpg": "PLACEHOLDER_URL_TRUNCADA",
    "img_11_22_30_31.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/z70jythjtvpf.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005242Z&X-Amz-Expires=3600&X-Amz-Signature=00ac6e57e13779e15a79ca04b8921581490f41e5afd55d162be92eab80679dbf&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_12_22_35_33.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/000jhm48ns3j.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005242Z&X-Amz-Expires=3600&X-Amz-Signature=df9222eede345436ada002df253a4bb6aedd1f77850dd8db1c8e44a294e08aa2&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_13_22_36_38.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/54rkwxceg0f1.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20261117T005242Z&X-Amz-Expires=3600&X-Amz-Signature=b44d3bd9444a3a5fa1b393b7a807ae0ffde9c9d41e33d7deb5ebb1b58ecf22c7&X-Amz-SignedHeaders=host&x-id=GetObject",
    "img_14_22_38_14.jpg": "https://temp.4d4f16c61d89ec64e760039c4ec50717.r2.cloudflarestorage.com/project_scope/ok_iXUpiDMo51f-/pr_LK89DHUUXKs5/files/h7ffzs37k51m.jpg?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=7685aa30fee07632b83ea58979a398d0%2F20261117T005241Z&X-Amz-Expires=3600&X-Amz-Signature=698b847f92fb2edc8874dc5bc979e2f162615bf78f84c3b63ba648317c87cdb1&X-Amz-SignedHeaders=host&x-id=GetObject",
}

if __name__ == "__main__":
    exitosos = 0
    for nombre, url in urls.items():
        if "PLACEHOLDER" in url:
            print(f"SALTANDO {nombre}: URL incompleta")
            continue
        try:
            urllib.request.urlretrieve(url, nombre)
            print(f"OK: {nombre} ({os.path.getsize(nombre)} bytes)")
            exitosos += 1
        except Exception as e:
            print(f"ERROR {nombre}: {e}")
    print(f"\nDescargados: {exitosos}/{len([u for u in urls.values() if 'PLACEHOLDER' not in u])}")