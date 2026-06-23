// Configuracion de la base del backend.
//
// Por defecto NO define nada: la web servida por el propio backend usa el mismo
// origen, y la APK pide la URL al usuario. Para "hornear" una URL por defecto
// (por ejemplo, tu Space de Hugging Face), descomenta y ajusta:
//
//   window.SCAVENGER_API_BASE = "https://usuario-scavenger.hf.space";
//
// El build de la APK (android.yml) reescribe este archivo automaticamente si
// defines la variable de repositorio SCAVENGER_API_BASE.
