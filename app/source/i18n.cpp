#include "i18n.h"

#include <coreinit/memory.h>
#include <coreinit/userconfig.h>

namespace {

Lang gLang = Lang::EN;

// Columns: JA, EN, FR, DE, IT, ES, ZH (simplified), KO, NL, PT, RU, TW (traditional)
const char *const kTexts[S_COUNT][(int) Lang::Count] = {
        /* S_SERVER_RUNNING */
        {"サーバー稼働中", "Server running", "Serveur actif", "Server läuft", "Server attivo", "Servidor activo",
         "服务器运行中", "서버 실행 중", "Server actief", "Servidor ativo", "Сервер работает", "伺服器執行中"},
        /* S_SERVER_STOPPED */
        {"サーバー停止", "Server stopped", "Serveur arrêté", "Server gestoppt", "Server fermo", "Servidor detenido",
         "服务器已停止", "서버 중지됨", "Server gestopt", "Servidor parado", "Сервер остановлен", "伺服器已停止"},
        /* S_STARTING */
        {"起動中…", "Starting…", "Démarrage…", "Wird gestartet …", "Avvio in corso…", "Iniciando…",
         "正在启动…", "시작하는 중…", "Bezig met starten…", "Iniciando…", "Запуск…", "正在啟動…"},
        /* S_TAB_DASHBOARD */
        {"ダッシュボード", "Dashboard", "Tableau de bord", "Übersicht", "Panoramica", "Panel",
         "概览", "대시보드", "Overzicht", "Painel", "Обзор", "總覽"},
        /* S_TAB_ACCESS */
        {"接続情報", "Credentials", "Identifiants", "Zugangsdaten", "Credenziali", "Credenciales",
         "登录信息", "접속 정보", "Inloggegevens", "Credenciais", "Доступ", "登入資訊"},
        /* S_TAB_DRIVES */
        {"ドライブ", "Drives", "Lecteurs", "Laufwerke", "Unità", "Unidades",
         "存储设备", "드라이브", "Schijven", "Unidades", "Накопители", "儲存裝置"},
        /* S_TAB_FILES */
        {"ファイル", "Files", "Fichiers", "Dateien", "File", "Archivos",
         "文件", "파일", "Bestanden", "Arquivos", "Файлы", "檔案"},
        /* S_HINT_PAGE */
        {"ページ切替", "Switch page", "Changer de page", "Seite wechseln", "Cambia pagina", "Cambiar página",
         "切换页面", "페이지 전환", "Pagina wisselen", "Mudar página", "Сменить страницу", "切換頁面"},
        /* S_HINT_QUIT */
        {"終了", "Quit", "Quitter", "Beenden", "Esci", "Salir",
         "退出", "종료", "Afsluiten", "Sair", "Выход", "結束"},
        /* S_HINT_OPEN */
        {"開く", "Open", "Ouvrir", "Öffnen", "Apri", "Abrir",
         "打开", "열기", "Openen", "Abrir", "Открыть", "開啟"},
        /* S_HINT_BACK */
        {"戻る", "Back", "Retour", "Zurück", "Indietro", "Atrás",
         "返回", "뒤로", "Terug", "Voltar", "Назад", "返回"},
        /* S_HINT_SHOW_PW */
        {"パスワードを表示", "Show password", "Afficher le mot de passe", "Passwort zeigen", "Mostra password",
         "Mostrar contraseña", "显示密码", "비밀번호 표시", "Wachtwoord tonen", "Mostrar senha", "Показать пароль",
         "顯示密碼"},
        /* S_HINT_HIDE_PW */
        {"パスワードを隠す", "Hide password", "Masquer le mot de passe", "Passwort verbergen", "Nascondi password",
         "Ocultar contraseña", "隐藏密码", "비밀번호 숨기기", "Wachtwoord verbergen", "Ocultar senha", "Скрыть пароль",
         "隱藏密碼"},
        /* S_HINT_NEW_PW */
        {"新しいパスワード", "New password", "Nouveau mot de passe", "Neues Passwort", "Nuova password",
         "Nueva contraseña", "新密码", "새 비밀번호", "Nieuw wachtwoord", "Nova senha", "Новый пароль", "新密碼"},
        /* S_HINT_CONFIRM_PW */
        {"もう一度押すと新しいパスワードを作成（接続は切断されます）",
         "Press again for a new password (disconnects everyone)",
         "Appuyez encore pour un nouveau mot de passe (déconnecte tout le monde)",
         "Nochmal drücken: neues Passwort (trennt alle Verbindungen)",
         "Premi di nuovo per una nuova password (disconnette tutti)",
         "Pulsa otra vez para una nueva contraseña (desconecta a todos)",
         "再按一次生成新密码（将断开所有连接）",
         "다시 누르면 새 비밀번호 생성 (모든 연결 끊김)",
         "Druk nogmaals voor een nieuw wachtwoord (verbreekt alle verbindingen)",
         "Pressione de novo para uma nova senha (desconecta todos)",
         "Нажмите ещё раз для нового пароля (все соединения будут разорваны)",
         "再按一次產生新密碼（將中斷所有連線）"},
        /* S_HINT_SELECT */
        {"選択", "Select", "Sélectionner", "Auswählen", "Seleziona", "Seleccionar",
         "选择", "선택", "Selecteren", "Selecionar", "Выбрать", "選擇"},
        /* S_SPEED */
        {"転送速度", "Speed", "Vitesse", "Geschwindigkeit", "Velocità", "Velocidad",
         "速度", "속도", "Snelheid", "Velocidade", "Скорость", "速度"},
        /* S_FROM_WIIU */
        {"Wii Uから", "From Wii U", "Depuis la Wii U", "Von der Wii U", "Dalla Wii U", "Desde Wii U",
         "来自 Wii U", "Wii U에서", "Van de Wii U", "Do Wii U", "С Wii U", "來自 Wii U"},
        /* S_TO_WIIU */
        {"Wii Uへ", "To Wii U", "Vers la Wii U", "Zur Wii U", "Alla Wii U", "Hacia Wii U",
         "传到 Wii U", "Wii U로", "Naar de Wii U", "Para o Wii U", "На Wii U", "傳到 Wii U"},
        /* S_LAST_60S */
        {"直近60秒", "Last 60 seconds", "60 dernières secondes", "Letzte 60 Sekunden", "Ultimi 60 secondi",
         "Últimos 60 segundos", "最近 60 秒", "최근 60초", "Laatste 60 seconden", "Últimos 60 segundos",
         "Последние 60 секунд", "最近 60 秒"},
        /* S_STATUS */
        {"状態", "Status", "État", "Status", "Stato", "Estado",
         "状态", "상태", "Status", "Status", "Состояние", "狀態"},
        /* S_CONNECTIONS */
        {"接続", "Connections", "Connexions", "Verbindungen", "Connessioni", "Conexiones",
         "连接", "연결", "Verbindingen", "Conexões", "Подключения", "連線"},
        /* S_TRANSFERRED */
        {"転送合計", "Transferred", "Transféré", "Übertragen", "Trasferiti", "Transferido",
         "已传输", "전송량", "Overgedragen", "Transferido", "Передано", "已傳輸"},
        /* S_UPTIME */
        {"稼働時間", "Uptime", "Durée d'activité", "Laufzeit", "Tempo attivo", "Tiempo activo",
         "运行时间", "가동 시간", "Looptijd", "Tempo ativo", "Время работы", "執行時間"},
        /* S_ACTIVE_TRANSFERS */
        {"転送中", "Current transfers", "Transferts en cours", "Aktuelle Übertragungen", "Trasferimenti in corso",
         "Transferencias actuales", "当前传输", "진행 중인 전송", "Huidige overdrachten", "Transferências atuais",
         "Текущие передачи", "目前傳輸"},
        /* S_NO_TRANSFERS */
        {"転送中のファイルはありません", "No transfers right now", "Aucun transfert en cours",
         "Gerade keine Übertragungen", "Nessun trasferimento in corso", "No hay transferencias", "暂无传输",
         "진행 중인 전송 없음", "Geen overdrachten", "Nenhuma transferência", "Нет активных передач", "目前沒有傳輸"},
        /* S_CONNECT_HINT */
        {"SFTPクライアントで {0} に接続してください", "Connect with an SFTP client to {0}",
         "Connectez un client SFTP à {0}", "Verbinde dich per SFTP mit {0}", "Connettiti con un client SFTP a {0}",
         "Conéctate con un cliente SFTP a {0}", "使用 SFTP 客户端连接 {0}", "SFTP 클라이언트로 {0}에 연결하세요",
         "Maak met een SFTP-client verbinding met {0}", "Conecte um cliente SFTP a {0}",
         "Подключитесь SFTP-клиентом к {0}", "使用 SFTP 用戶端連線至 {0}"},
        /* S_IDLE */
        {"待機中", "idle", "inactif", "bereit", "inattivo", "inactivo",
         "空闲", "대기 중", "inactief", "ocioso", "ожидание", "閒置"},
        /* S_LOGGING_IN */
        {"ログイン中…", "signing in…", "connexion…", "Anmeldung läuft …", "accesso in corso…", "iniciando sesión…",
         "正在登录…", "로그인 중…", "bezig met inloggen…", "entrando…", "вход…", "正在登入…"},
        /* S_RECEIVED */
        {"{0} 受信", "{0} received", "{0} reçus", "{0} empfangen", "{0} ricevuti", "{0} recibidos",
         "已接收 {0}", "{0} 수신", "{0} ontvangen", "{0} recebidos", "получено {0}", "已接收 {0}"},
        /* S_EVENTS */
        {"イベント", "Events", "Événements", "Ereignisse", "Eventi", "Eventos",
         "事件", "이벤트", "Gebeurtenissen", "Eventos", "События", "事件"},
        /* S_NO_EVENTS */
        {"まだイベントはありません", "No events yet", "Aucun événement", "Noch keine Ereignisse", "Nessun evento",
         "Aún no hay eventos", "暂无事件", "이벤트 없음", "Nog geen gebeurtenissen", "Nenhum evento ainda",
         "Событий пока нет", "尚無事件"},
        /* S_RECENT */
        {"最近の転送", "Recent transfers", "Transferts récents", "Zuletzt übertragen", "Trasferimenti recenti",
         "Transferencias recientes", "最近传输", "최근 전송", "Recente overdrachten", "Transferências recentes",
         "Недавние передачи", "最近傳輸"},
        /* S_NO_RECENT */
        {"まだ転送はありません", "Nothing transferred yet", "Aucun transfert", "Noch nichts übertragen",
         "Ancora nessun trasferimento", "Aún no hay transferencias", "暂无传输记录", "전송 기록 없음",
         "Nog niets overgedragen", "Nada transferido ainda", "Пока ничего не передано", "尚無傳輸記錄"},
        /* S_CONNECTION_DATA */
        {"接続情報", "Connection details", "Informations de connexion", "Verbindungsdaten", "Dati di connessione",
         "Datos de conexión", "连接信息", "연결 정보", "Verbindingsgegevens", "Dados de conexão",
         "Данные подключения", "連線資訊"},
        /* S_PROTOCOL */
        {"プロトコル", "Protocol", "Protocole", "Protokoll", "Protocollo", "Protocolo",
         "协议", "프로토콜", "Protocol", "Protocolo", "Протокол", "通訊協定"},
        /* S_ADDRESS */
        {"アドレス", "Address", "Adresse", "Adresse", "Indirizzo", "Dirección",
         "地址", "주소", "Adres", "Endereço", "Адрес", "位址"},
        /* S_PORT */
        {"ポート", "Port", "Port", "Port", "Porta", "Puerto",
         "端口", "포트", "Poort", "Porta", "Порт", "連接埠"},
        /* S_USERNAME */
        {"ユーザー名", "Username", "Nom d'utilisateur", "Benutzername", "Nome utente", "Usuario",
         "用户名", "사용자 이름", "Gebruikersnaam", "Usuário", "Имя пользователя", "使用者名稱"},
        /* S_ANY_USER */
        {"任意", "any", "au choix", "beliebig", "qualsiasi", "cualquiera",
         "任意", "아무거나", "willekeurig", "qualquer", "любое", "任意"},
        /* S_PASSWORD */
        {"パスワード", "Password", "Mot de passe", "Passwort", "Password", "Contraseña",
         "密码", "비밀번호", "Wachtwoord", "Senha", "Пароль", "密碼"},
        /* S_PASSWORD_OFF */
        {"オフ（鍵認証のみ）", "off (keys only)", "désactivé (clés uniquement)", "aus (nur Schlüssel)",
         "disattivata (solo chiavi)", "desactivada (solo claves)", "关闭（仅限密钥）", "꺼짐 (키 전용)",
         "uit (alleen sleutels)", "desativada (só chaves)", "выкл. (только ключи)", "關閉（僅限金鑰）"},
        /* S_NO_NETWORK */
        {"ネットワーク未接続", "no network connection", "pas de connexion réseau", "keine Netzwerkverbindung",
         "nessuna connessione di rete", "sin conexión de red", "未连接网络", "네트워크 연결 없음",
         "geen netwerkverbinding", "sem conexão de rede", "нет сети", "未連線網路"},
        /* S_SECURITY */
        {"セキュリティ", "Security", "Sécurité", "Sicherheit", "Sicurezza", "Seguridad",
         "安全", "보안", "Beveiliging", "Segurança", "Безопасность", "安全性"},
        /* S_HOSTKEY */
        {"ホスト鍵", "Host key", "Clé d'hôte", "Host-Key", "Chiave host", "Clave de host",
         "主机密钥", "호스트 키", "Hostsleutel", "Chave do host", "Ключ хоста", "主機金鑰"},
        /* S_HOSTKEY_HINT */
        {"初回接続時にこの指紋を確認してください", "Compare this fingerprint on your first connection",
         "Comparez cette empreinte lors de la première connexion", "Vergleiche diesen Fingerabdruck beim ersten Verbinden",
         "Confronta questa impronta al primo collegamento", "Compara esta huella en la primera conexión",
         "首次连接时请核对此指纹", "처음 연결할 때 이 지문을 확인하세요",
         "Vergelijk deze vingerafdruk bij de eerste verbinding", "Compare esta impressão digital na primeira conexão",
         "Сверьте этот отпечаток при первом подключении", "首次連線時請核對此指紋"},
        /* S_KEY_LOGIN */
        {"鍵認証", "Key login", "Connexion par clé", "Schlüssel-Login", "Accesso con chiave", "Acceso con clave",
         "密钥登录", "키 로그인", "Inloggen met sleutel", "Login por chave", "Вход по ключу", "金鑰登入"},
        /* S_KEYS_COUNT */
        {"{0} 個の鍵", "{0} key(s)", "{0} clé(s)", "{0} Schlüssel", "{0} chiave/i", "{0} clave(s)",
         "{0} 个密钥", "키 {0}개", "{0} sleutel(s)", "{0} chave(s)", "ключей: {0}", "{0} 個金鑰"},
        /* S_KEYS_NONE */
        {"鍵は未登録", "no keys stored", "aucune clé", "keine Schlüssel hinterlegt", "nessuna chiave",
         "ninguna clave", "未添加密钥", "등록된 키 없음", "geen sleutels", "nenhuma chave", "ключей нет", "未加入金鑰"},
        /* S_SETTINGS */
        {"設定", "Settings", "Paramètres", "Einstellungen", "Impostazioni", "Ajustes",
         "设置", "설정", "Instellingen", "Configurações", "Настройки", "設定"},
        /* S_FROM_PLUGIN */
        {"プラグインから取得（Aromaメニューで変更）", "from the plugin (change them in the Aroma menu)",
         "du plugin (à modifier dans le menu Aroma)", "vom Plugin (ändern im Aroma-Menü)",
         "dal plugin (modificale nel menu Aroma)", "del plugin (cámbialos en el menú de Aroma)",
         "来自插件（在 Aroma 菜单中修改）", "플러그인 설정 (Aroma 메뉴에서 변경)",
         "van de plug-in (wijzigen in het Aroma-menu)", "do plugin (altere no menu do Aroma)",
         "из плагина (изменяются в меню Aroma)", "來自外掛（在 Aroma 選單中修改）"},
        /* S_FROM_APP */
        {"アプリ独自: SD:/wiiu/sftpiiu/app.cfg", "own: SD:/wiiu/sftpiiu/app.cfg", "propres : SD:/wiiu/sftpiiu/app.cfg",
         "eigene: SD:/wiiu/sftpiiu/app.cfg", "proprie: SD:/wiiu/sftpiiu/app.cfg", "propios: SD:/wiiu/sftpiiu/app.cfg",
         "独立: SD:/wiiu/sftpiiu/app.cfg", "자체: SD:/wiiu/sftpiiu/app.cfg", "eigen: SD:/wiiu/sftpiiu/app.cfg",
         "próprias: SD:/wiiu/sftpiiu/app.cfg", "собственные: SD:/wiiu/sftpiiu/app.cfg", "獨立: SD:/wiiu/sftpiiu/app.cfg"},
        /* S_LOGINS */
        {"ログイン履歴", "Sign-ins", "Connexions", "Anmeldungen", "Accessi", "Inicios de sesión",
         "登录记录", "로그인 기록", "Aanmeldingen", "Logins", "Входы", "登入記錄"},
        /* S_NO_LOGINS */
        {"まだログインはありません", "No sign-ins yet", "Aucune connexion", "Noch keine Anmeldungen", "Nessun accesso",
         "Aún no hay inicios de sesión", "暂无登录", "로그인 기록 없음", "Nog geen aanmeldingen", "Nenhum login ainda",
         "Входов пока нет", "尚無登入"},
        /* S_METHOD_KEY */
        {"鍵", "Key", "Clé", "Schlüssel", "Chiave", "Clave",
         "密钥", "키", "Sleutel", "Chave", "Ключ", "金鑰"},
        /* S_DRIVE_SD */
        {"SDカード", "SD card", "Carte SD", "SD-Karte", "Scheda SD", "Tarjeta SD",
         "SD 卡", "SD 카드", "SD-kaart", "Cartão SD", "SD-карта", "SD 卡"},
        /* S_DRIVE_MLC */
        {"本体保存メモリー (MLC)", "System memory (MLC)", "Mémoire système (MLC)", "Interner Speicher (MLC)",
         "Memoria interna (MLC)", "Memoria interna (MLC)", "主机存储 (MLC)", "본체 저장 메모리 (MLC)",
         "Systeemgeheugen (MLC)", "Memória interna (MLC)", "Внутренняя память (MLC)", "主機儲存 (MLC)"},
        /* S_DRIVE_USB */
        {"USBストレージ", "USB storage", "Stockage USB", "USB-Speicher", "Memoria USB", "Almacenamiento USB",
         "USB 存储", "USB 저장 장치", "USB-opslag", "Armazenamento USB", "USB-накопитель", "USB 儲存"},
        /* S_DRIVE_SLC */
        {"システム領域 (SLC)", "System area (SLC)", "Zone système (SLC)", "Systembereich (SLC)",
         "Area di sistema (SLC)", "Área del sistema (SLC)", "系统区 (SLC)", "시스템 영역 (SLC)",
         "Systeemgebied (SLC)", "Área do sistema (SLC)", "Системная область (SLC)", "系統區 (SLC)"},
        /* S_DRIVE_SLCCMPT */
        {"vWii システム領域", "vWii system area", "Zone système vWii", "vWii-Systembereich", "Area di sistema vWii",
         "Área del sistema vWii", "vWii 系统区", "vWii 시스템 영역", "vWii-systeemgebied", "Área do sistema vWii",
         "Системная область vWii", "vWii 系統區"},
        /* S_DRIVE_DISC */
        {"ディスク", "Disc", "Disque", "Disc", "Disco", "Disco",
         "光盘", "디스크", "Disc", "Disco", "Диск", "光碟"},
        /* S_READ_WRITE */
        {"読み書き", "Read & write", "Lecture et écriture", "Lesen & Schreiben", "Lettura e scrittura",
         "Lectura y escritura", "读写", "읽기 및 쓰기", "Lezen en schrijven", "Leitura e gravação", "Чтение и запись",
         "讀寫"},
        /* S_READ_ONLY */
        {"読み取り専用", "Read only", "Lecture seule", "Nur lesen", "Sola lettura", "Solo lectura",
         "只读", "읽기 전용", "Alleen lezen", "Somente leitura", "Только чтение", "唯讀"},
        /* S_FREE_OF */
        {"{1} 中 {0} 空き", "{0} free of {1}", "{0} libres sur {1}", "{0} frei von {1}", "{0} liberi su {1}",
         "{0} libres de {1}", "可用 {0}，共 {1}", "{1} 중 {0} 사용 가능", "{0} vrij van {1}", "{0} livres de {1}",
         "свободно {0} из {1}", "可用 {0}，共 {1}"},
        /* S_SPACE_UNKNOWN */
        {"空き容量不明", "free space unknown", "espace libre inconnu", "freier Platz unbekannt",
         "spazio libero sconosciuto", "espacio libre desconocido", "可用空间未知", "남은 공간 알 수 없음",
         "vrije ruimte onbekend", "espaço livre desconhecido", "свободное место неизвестно", "可用空間未知"},
        /* S_NEED_MOCHA */
        {"MLC・USB・SLCにはMochaPayloadが必要です", "MLC, USB and SLC need the MochaPayload",
         "MLC, USB et SLC nécessitent MochaPayload", "MLC, USB und SLC brauchen die MochaPayload",
         "MLC, USB e SLC richiedono MochaPayload", "MLC, USB y SLC necesitan MochaPayload",
         "MLC、USB 和 SLC 需要 MochaPayload", "MLC, USB, SLC에는 MochaPayload가 필요합니다",
         "MLC, USB en SLC hebben de MochaPayload nodig", "MLC, USB e SLC precisam do MochaPayload",
         "Для MLC, USB и SLC нужен MochaPayload", "MLC、USB 與 SLC 需要 MochaPayload"},
        /* S_FOLDER */
        {"フォルダー", "Folder", "Dossier", "Ordner", "Cartella", "Carpeta",
         "文件夹", "폴더", "Map", "Pasta", "Папка", "資料夾"},
        /* S_FILE */
        {"ファイル", "File", "Fichier", "Datei", "File", "Archivo",
         "文件", "파일", "Bestand", "Arquivo", "Файл", "檔案"},
        /* S_EMPTY_FOLDER */
        {"このフォルダーは空です", "This folder is empty", "Ce dossier est vide", "Dieser Ordner ist leer",
         "Questa cartella è vuota", "Esta carpeta está vacía", "此文件夹为空", "빈 폴더입니다", "Deze map is leeg",
         "Esta pasta está vazia", "Папка пуста", "此資料夾是空的"},
        /* S_CANNOT_OPEN */
        {"フォルダーを開けません", "Cannot open this folder", "Impossible d'ouvrir ce dossier",
         "Ordner kann nicht geöffnet werden", "Impossibile aprire la cartella", "No se puede abrir la carpeta",
         "无法打开此文件夹", "폴더를 열 수 없습니다", "Map kan niet worden geopend", "Não foi possível abrir a pasta",
         "Не удаётся открыть папку", "無法開啟此資料夾"},
        /* S_ITEMS */
        {"{0} 項目", "{0} items", "{0} éléments", "{0} Einträge", "{0} elementi", "{0} elementos",
         "{0} 项", "{0}개 항목", "{0} items", "{0} itens", "элементов: {0}", "{0} 個項目"},
        /* S_ITEMS_ONE */
        {"1 項目", "1 item", "1 élément", "1 Eintrag", "1 elemento", "1 elemento",
         "1 项", "1개 항목", "1 item", "1 item", "1 элемент", "1 個項目"},
        /* S_ACCESS_RIGHTS */
        {"アクセス", "Access", "Accès", "Zugriff", "Accesso", "Acceso",
         "访问权限", "접근 권한", "Toegang", "Acesso", "Доступ", "存取權限"},
        /* S_SIZE */
        {"サイズ", "Size", "Taille", "Größe", "Dimensione", "Tamaño",
         "大小", "크기", "Grootte", "Tamanho", "Размер", "大小"},
        /* S_MODIFIED */
        {"更新日時", "Modified", "Modifié", "Geändert", "Modificato", "Modificado",
         "修改时间", "수정한 날짜", "Gewijzigd", "Modificado", "Изменён", "修改時間"},
        /* S_TYPE */
        {"種類", "Type", "Type", "Typ", "Tipo", "Tipo",
         "类型", "종류", "Type", "Tipo", "Тип", "類型"},
        /* S_LOCATION */
        {"場所", "Location", "Emplacement", "Ort", "Percorso", "Ubicación",
         "位置", "위치", "Locatie", "Local", "Расположение", "位置"},
        /* S_TRUNCATED */
        {"最初の {0} 項目のみ表示", "Showing the first {0} items", "Seuls les {0} premiers éléments sont affichés",
         "Nur die ersten {0} Einträge", "Solo i primi {0} elementi", "Solo los primeros {0} elementos",
         "仅显示前 {0} 项", "처음 {0}개 항목만 표시", "Alleen de eerste {0} items", "Apenas os primeiros {0} itens",
         "Показаны первые {0}", "僅顯示前 {0} 個項目"},
        /* S_LOADING */
        {"読み込み中…", "Loading…", "Chargement…", "Wird geladen …", "Caricamento…", "Cargando…",
         "正在加载…", "불러오는 중…", "Bezig met laden…", "Carregando…", "Загрузка…", "正在載入…"},
        /* S_E_STARTED */
        {"サーバー起動（ポート {0}）", "Server started on port {0}", "Serveur démarré sur le port {0}",
         "Server gestartet (Port {0})", "Server avviato sulla porta {0}", "Servidor iniciado en el puerto {0}",
         "服务器已在端口 {0} 启动", "포트 {0}에서 서버 시작", "Server gestart op poort {0}",
         "Servidor iniciado na porta {0}", "Сервер запущен на порту {0}", "伺服器已在連接埠 {0} 啟動"},
        /* S_E_LOGIN_PW */
        {"{0} がログイン（パスワード）", "{0} signed in (password)", "{0} connecté (mot de passe)",
         "{0} angemeldet (Passwort)", "{0} ha effettuato l'accesso (password)", "{0} inició sesión (contraseña)",
         "{0} 已登录（密码）", "{0} 로그인 (비밀번호)", "{0} aangemeld (wachtwoord)", "{0} entrou (senha)",
         "{0} вошёл (пароль)", "{0} 已登入（密碼）"},
        /* S_E_LOGIN_KEY */
        {"{0} がログイン（鍵）", "{0} signed in (key)", "{0} connecté (clé)", "{0} angemeldet (Schlüssel)",
         "{0} ha effettuato l'accesso (chiave)", "{0} inició sesión (clave)", "{0} 已登录（密钥）",
         "{0} 로그인 (키)", "{0} aangemeld (sleutel)", "{0} entrou (chave)", "{0} вошёл (ключ)", "{0} 已登入（金鑰）"},
        /* S_E_DISCONNECTED */
        {"{0} が切断", "{0} disconnected", "{0} déconnecté", "{0} getrennt", "{0} disconnesso", "{0} desconectado",
         "{0} 已断开", "{0} 연결 끊김", "{0} verbroken", "{0} desconectado", "{0} отключился", "{0} 已中斷"},
        /* S_E_WRONG_PW */
        {"{0} からの誤ったパスワード", "Wrong password from {0}", "Mot de passe incorrect de {0}",
         "Falsches Passwort von {0}", "Password errata da {0}", "Contraseña incorrecta de {0}", "{0} 密码错误",
         "{0}의 잘못된 비밀번호", "Onjuist wachtwoord van {0}", "Senha incorreta de {0}", "Неверный пароль от {0}",
         "{0} 密碼錯誤"},
        /* S_E_LOCKED */
        {"失敗が多すぎます。パスワードログインを5分間停止", "Too many failures, password login locked for 5 min",
         "Trop d'échecs, connexion par mot de passe bloquée 5 min",
         "Zu viele Fehlversuche, Passwort-Login 5 Min. gesperrt",
         "Troppi tentativi, accesso con password bloccato per 5 min",
         "Demasiados fallos, acceso con contraseña bloqueado 5 min", "失败次数过多，密码登录锁定 5 分钟",
         "실패가 너무 많아 비밀번호 로그인 5분 잠금",
         "Te veel mislukte pogingen, inloggen met wachtwoord 5 min geblokkeerd",
         "Muitas falhas, login por senha bloqueado por 5 min",
         "Слишком много ошибок, вход по паролю заблокирован на 5 мин", "失敗次數過多，密碼登入鎖定 5 分鐘"},
        /* S_E_TOO_MANY */
        {"{0} を拒否（接続数が上限）", "{0} rejected: too many connections", "{0} refusé : trop de connexions",
         "{0} abgelehnt: zu viele Verbindungen", "{0} rifiutato: troppe connessioni",
         "{0} rechazado: demasiadas conexiones", "{0} 被拒绝：连接过多", "{0} 거부됨: 연결이 너무 많음",
         "{0} geweigerd: te veel verbindingen", "{0} recusado: conexões demais",
         "{0} отклонён: слишком много подключений", "{0} 已拒絕：連線過多"},
        /* S_E_NEW_PW */
        {"新しいパスワードを作成しました", "New password created", "Nouveau mot de passe créé", "Neues Passwort erzeugt",
         "Nuova password creata", "Nueva contraseña creada", "已生成新密码", "새 비밀번호 생성됨",
         "Nieuw wachtwoord aangemaakt", "Nova senha criada", "Создан новый пароль", "已產生新密碼"},
        /* S_ERR_SELFTEST */
        {"暗号セルフテスト失敗（{0}）。サーバーは停止したままです", "Crypto self-test failed ({0}), server stays off",
         "Échec de l'autotest crypto ({0}), serveur arrêté", "Krypto-Selbsttest fehlgeschlagen ({0}), Server bleibt aus",
         "Autotest crittografico fallito ({0}), server spento",
         "Falló la autoprueba criptográfica ({0}), servidor apagado", "加密自检失败（{0}），服务器未启动",
         "암호 자체 테스트 실패 ({0}), 서버 꺼짐", "Crypto-zelftest mislukt ({0}), server blijft uit",
         "Autoteste de criptografia falhou ({0}), servidor desligado",
         "Самопроверка шифрования не пройдена ({0}), сервер выключен", "加密自我測試失敗（{0}），伺服器未啟動"},
        /* S_ERR_PORT */
        {"ポート {0} は使用中です。別のサーバーが動作していませんか？", "Port {0} is in use. Is another server running?",
         "Le port {0} est occupé. Un autre serveur est-il actif ?", "Port {0} ist belegt. Läuft schon ein anderer Server?",
         "La porta {0} è occupata. C'è già un altro server?",
         "El puerto {0} está ocupado. ¿Hay otro servidor activo?", "端口 {0} 已被占用。是否有其他服务器在运行？",
         "포트 {0}이(가) 사용 중입니다. 다른 서버가 실행 중인가요?",
         "Poort {0} is bezet. Draait er al een andere server?", "A porta {0} está ocupada. Outro servidor está ativo?",
         "Порт {0} занят. Уже работает другой сервер?", "連接埠 {0} 已被使用。是否有其他伺服器正在執行？"},
        /* S_AGO_S */
        {"{0}秒前", "{0} s ago", "il y a {0} s", "vor {0} s", "{0} s fa", "hace {0} s",
         "{0} 秒前", "{0}초 전", "{0} s geleden", "há {0} s", "{0} с назад", "{0} 秒前"},
        /* S_AGO_MIN */
        {"{0}分前", "{0} min ago", "il y a {0} min", "vor {0} min", "{0} min fa", "hace {0} min",
         "{0} 分钟前", "{0}분 전", "{0} min geleden", "há {0} min", "{0} мин назад", "{0} 分鐘前"},
        /* S_AGO_H */
        {"{0}時間前", "{0} h ago", "il y a {0} h", "vor {0} h", "{0} h fa", "hace {0} h",
         "{0} 小时前", "{0}시간 전", "{0} u geleden", "há {0} h", "{0} ч назад", "{0} 小時前"},
        /* S_IN_TIME */
        {"{0}（{1}）", "{0} in {1}", "{0} en {1}", "{0} in {1}", "{0} in {1}", "{0} en {1}",
         "{0}，用时 {1}", "{0} ({1})", "{0} in {1}", "{0} em {1}", "{0} за {1}", "{0}，耗時 {1}"},

        // ------------------------------------------------------------ settings
        /* S_ON */
        {"オン", "On", "Activé", "An", "Attivo", "Activado", "开", "켬", "Aan", "Ativado", "Вкл.", "開"},
        /* S_OFF */
        {"オフ", "Off", "Désactivé", "Aus", "Disattivo", "Desactivado", "关", "끔", "Uit", "Desativado", "Выкл.", "關"},
        /* S_OPT_FILEOPS */
        {"ファイルの変更を許可", "Allow file changes", "Autoriser les modifications", "Dateien ändern erlauben",
         "Consenti modifiche ai file", "Permitir cambios en archivos", "允许修改文件", "파일 변경 허용",
         "Bestandswijzigingen toestaan", "Permitir alterações de arquivos", "Разрешить изменение файлов", "允許修改檔案"},
        /* S_OPT_FILEOPS_DESC */
        {"削除・名前の変更・コピー・移動・エディターでの保存", "Delete, rename, copy, move and save in the editor",
         "Supprimer, renommer, copier, déplacer et enregistrer dans l'éditeur",
         "Löschen, Umbenennen, Kopieren, Verschieben und Speichern im Editor",
         "Elimina, rinomina, copia, sposta e salva nell'editor",
         "Eliminar, renombrar, copiar, mover y guardar en el editor", "删除、重命名、复制、移动以及在编辑器中保存",
         "삭제, 이름 변경, 복사, 이동 및 편집기에서 저장",
         "Verwijderen, hernoemen, kopiëren, verplaatsen en opslaan in de editor",
         "Excluir, renomear, copiar, mover e salvar no editor",
         "Удаление, переименование, копирование, перемещение и сохранение в редакторе",
         "刪除、重新命名、複製、移動以及在編輯器中儲存"},
        /* S_OPT_HIDDEN */
        {"隠しファイルを表示", "Show hidden files", "Afficher les fichiers cachés", "Versteckte Dateien anzeigen",
         "Mostra file nascosti", "Mostrar archivos ocultos", "显示隐藏文件", "숨김 파일 표시", "Verborgen bestanden tonen",
         "Mostrar arquivos ocultos", "Показывать скрытые файлы", "顯示隱藏檔案"},
        /* S_OPT_HIDDEN_DESC */
        {"名前がピリオドで始まるファイルとフォルダー", "Files and folders whose name starts with a dot",
         "Fichiers et dossiers dont le nom commence par un point",
         "Dateien und Ordner, deren Name mit einem Punkt beginnt",
         "File e cartelle il cui nome inizia con un punto",
         "Archivos y carpetas cuyo nombre empieza por un punto", "名称以点开头的文件和文件夹",
         "이름이 점으로 시작하는 파일과 폴더", "Bestanden en mappen waarvan de naam met een punt begint",
         "Arquivos e pastas cujo nome começa com um ponto", "Файлы и папки, имя которых начинается с точки",
         "名稱以點開頭的檔案和資料夾"},
        /* S_OPT_LANGUAGE */
        {"言語", "Language", "Langue", "Sprache", "Lingua", "Idioma", "语言", "언어", "Taal", "Idioma", "Язык", "語言"},
        /* S_OPT_LANGUAGE_DESC */
        {"表示言語", "Language of the interface", "Langue de l'interface", "Sprache der Oberfläche",
         "Lingua dell'interfaccia", "Idioma de la interfaz", "界面语言", "화면 언어", "Taal van de interface",
         "Idioma da interface", "Язык интерфейса", "介面語言"},
        /* S_LANG_AUTO */
        {"自動（本体の設定）", "Automatic (console)", "Automatique (console)", "Automatisch (Konsole)",
         "Automatica (console)", "Automático (consola)", "自动（主机设置）", "자동 (본체 설정)", "Automatisch (console)",
         "Automático (console)", "Автоматически (консоль)", "自動（主機設定）"},
        /* S_SETTINGS_SERVER_PLUGIN */
        {"ポート・パスワード・システムへのアクセスはAromaメニューのプラグインで設定します",
         "Port, password and system access are set for the plugin in the Aroma menu",
         "Le port, le mot de passe et l'accès système se règlent pour le plugin dans le menu Aroma",
         "Port, Passwort und Systemzugriff stellst du im Aroma-Menü beim Plugin ein",
         "Porta, password e accesso al sistema si impostano per il plugin nel menu Aroma",
         "El puerto, la contraseña y el acceso al sistema se ajustan para el plugin en el menú de Aroma",
         "端口、密码和系统访问请在 Aroma 菜单的插件中设置",
         "포트, 비밀번호, 시스템 접근은 Aroma 메뉴의 플러그인에서 설정합니다",
         "Poort, wachtwoord en systeemtoegang stel je in het Aroma-menu bij de plug-in in",
         "Porta, senha e acesso ao sistema são definidos no plugin pelo menu do Aroma",
         "Порт, пароль и доступ к системе настраиваются для плагина в меню Aroma",
         "連接埠、密碼和系統存取請在 Aroma 選單的外掛中設定"},
        /* S_SETTINGS_SERVER_APP */
        {"ポートとパスワード: SD:/wiiu/sftpiiu/app.cfg", "Port and password: SD:/wiiu/sftpiiu/app.cfg",
         "Port et mot de passe : SD:/wiiu/sftpiiu/app.cfg", "Port und Passwort: SD:/wiiu/sftpiiu/app.cfg",
         "Porta e password: SD:/wiiu/sftpiiu/app.cfg", "Puerto y contraseña: SD:/wiiu/sftpiiu/app.cfg",
         "端口和密码：SD:/wiiu/sftpiiu/app.cfg", "포트와 비밀번호: SD:/wiiu/sftpiiu/app.cfg",
         "Poort en wachtwoord: SD:/wiiu/sftpiiu/app.cfg", "Porta e senha: SD:/wiiu/sftpiiu/app.cfg",
         "Порт и пароль: SD:/wiiu/sftpiiu/app.cfg", "連接埠和密碼：SD:/wiiu/sftpiiu/app.cfg"},
        /* S_FILEOPS_WARN_TITLE */
        {"ファイルの変更を許可しますか？", "Allow file changes?", "Autoriser les modifications ?",
         "Dateien ändern erlauben?", "Consentire le modifiche ai file?", "¿Permitir cambios en archivos?",
         "允许修改文件吗？", "파일 변경을 허용할까요?", "Bestandswijzigingen toestaan?",
         "Permitir alterações de arquivos?", "Разрешить изменение файлов?", "允許修改檔案嗎？"},
        /* S_FILEOPS_WARN */
        {"削除したファイルは元に戻せません。SLCなどの読み取り専用領域は保護されたままです。",
         "Deleted files cannot be restored. Read-only areas such as the SLC stay protected.",
         "Les fichiers supprimés ne peuvent pas être restaurés. Les zones en lecture seule comme la SLC restent protégées.",
         "Gelöschte Dateien lassen sich nicht wiederherstellen. Schreibgeschützte Bereiche wie die SLC bleiben geschützt.",
         "I file eliminati non si possono recuperare. Le aree di sola lettura come la SLC restano protette.",
         "Los archivos eliminados no se pueden recuperar. Las áreas de solo lectura como la SLC siguen protegidas.",
         "删除的文件无法恢复。SLC 等只读区域仍受保护。",
         "삭제한 파일은 복구할 수 없습니다. SLC 같은 읽기 전용 영역은 계속 보호됩니다.",
         "Verwijderde bestanden kunnen niet worden hersteld. Alleen-lezen gebieden zoals de SLC blijven beschermd.",
         "Arquivos excluídos não podem ser recuperados. Áreas somente leitura como a SLC continuam protegidas.",
         "Удалённые файлы нельзя восстановить. Области только для чтения, например SLC, остаются защищены.",
         "刪除的檔案無法復原。SLC 等唯讀區域仍受保護。"},
        /* S_ENABLE */
        {"有効にする", "Enable", "Activer", "Aktivieren", "Attiva", "Activar", "启用", "사용", "Inschakelen", "Ativar",
         "Включить", "啟用"},
        /* S_CANCEL */
        {"キャンセル", "Cancel", "Annuler", "Abbrechen", "Annulla", "Cancelar", "取消", "취소", "Annuleren", "Cancelar",
         "Отмена", "取消"},
        /* S_HINT_MENU */
        {"メニュー", "Menu", "Menu", "Aktionen", "Menu", "Menú", "菜单", "메뉴", "Menu", "Menu", "Меню", "選單"},
        /* S_HINT_CHANGE */
        {"変更", "Change", "Modifier", "Ändern", "Cambia", "Cambiar", "更改", "변경", "Wijzigen", "Alterar", "Изменить",
         "變更"},
        /* S_HINT_ENABLE_IN_SETTINGS */
        {"変更は設定で有効にできます", "Enable changes in Settings", "Activez les modifications dans les paramètres",
         "Ändern in den Einstellungen aktivieren", "Attiva le modifiche nelle impostazioni",
         "Activa los cambios en los ajustes", "可在设置中启用修改", "설정에서 변경을 허용하세요",
         "Schakel wijzigingen in bij Instellingen", "Ative as alterações nas configurações",
         "Включите изменения в настройках", "可在設定中啟用修改"},

        // ------------------------------------------------------------ file actions
        /* S_ACT_RENAME */
        {"名前を変更", "Rename", "Renommer", "Umbenennen", "Rinomina", "Renombrar", "重命名", "이름 변경",
         "Naam wijzigen", "Renomear", "Переименовать", "重新命名"},
        /* S_ACT_DELETE */
        {"削除", "Delete", "Supprimer", "Löschen", "Elimina", "Eliminar", "删除", "삭제", "Verwijderen", "Excluir",
         "Удалить", "刪除"},
        /* S_ACT_COPY */
        {"コピー", "Copy", "Copier", "Kopieren", "Copia", "Copiar", "复制", "복사", "Kopiëren", "Copiar", "Копировать",
         "複製"},
        /* S_ACT_CUT */
        {"切り取り", "Cut", "Couper", "Ausschneiden", "Taglia", "Cortar", "剪切", "잘라내기", "Knippen", "Recortar",
         "Вырезать", "剪下"},
        /* S_ACT_PASTE */
        {"ここに貼り付け", "Paste here", "Coller ici", "Hier einfügen", "Incolla qui", "Pegar aquí", "粘贴到此处",
         "여기에 붙여넣기", "Hier plakken", "Colar aqui", "Вставить сюда", "貼上到此處"},
        /* S_ACT_NEWFOLDER */
        {"新しいフォルダー", "New folder", "Nouveau dossier", "Neuer Ordner", "Nuova cartella", "Nueva carpeta",
         "新建文件夹", "새 폴더", "Nieuwe map", "Nova pasta", "Новая папка", "新增資料夾"},
        /* S_ACT_NEWFILE */
        {"新しいテキストファイル", "New text file", "Nouveau fichier texte", "Neue Textdatei", "Nuovo file di testo",
         "Nuevo archivo de texto", "新建文本文件", "새 텍스트 파일", "Nieuw tekstbestand", "Novo arquivo de texto",
         "Новый текстовый файл", "新增文字檔"},
        /* S_CONFIRM_DELETE */
        {"「{0}」を削除しますか？", "Delete “{0}”?", "Supprimer « {0} » ?", "„{0}“ löschen?", "Eliminare «{0}»?",
         "¿Eliminar «{0}»?", "删除“{0}”？", "'{0}'을(를) 삭제할까요?", "‘{0}’ verwijderen?", "Excluir “{0}”?",
         "Удалить «{0}»?", "刪除「{0}」？"},
        /* S_CONFIRM_DELETE_DIR */
        {"フォルダーとその中身がすべて削除されます。", "The folder and everything in it will be deleted.",
         "Le dossier et tout son contenu seront supprimés.", "Der Ordner wird mit seinem gesamten Inhalt gelöscht.",
         "La cartella e tutto il suo contenuto verranno eliminati.", "Se eliminará la carpeta con todo su contenido.",
         "将删除该文件夹及其全部内容。", "폴더와 그 안의 모든 항목이 삭제됩니다.",
         "De map en alles erin wordt verwijderd.", "A pasta e todo o seu conteúdo serão excluídos.",
         "Папка будет удалена вместе со всем содержимым.", "將刪除此資料夾及其所有內容。"},
        /* S_CONFIRM_DELETE_FILE */
        {"ファイルは完全に削除されます。", "The file will be deleted permanently.",
         "Le fichier sera supprimé définitivement.", "Die Datei wird endgültig gelöscht.",
         "Il file verrà eliminato definitivamente.", "El archivo se eliminará definitivamente.",
         "该文件将被永久删除。", "파일이 영구적으로 삭제됩니다.", "Het bestand wordt definitief verwijderd.",
         "O arquivo será excluído permanentemente.", "Файл будет удалён без возможности восстановления.",
         "此檔案將被永久刪除。"},
        /* S_NEW_NAME */
        {"新しい名前", "New name", "Nouveau nom", "Neuer Name", "Nuovo nome", "Nuevo nombre", "新名称", "새 이름",
         "Nieuwe naam", "Novo nome", "Новое имя", "新名稱"},
        /* S_COPYING */
        {"コピー中…", "Copying…", "Copie…", "Wird kopiert …", "Copia in corso…", "Copiando…", "正在复制…",
         "복사하는 중…", "Bezig met kopiëren…", "Copiando…", "Копирование…", "正在複製…"},
        /* S_MOVING */
        {"移動中…", "Moving…", "Déplacement…", "Wird verschoben …", "Spostamento in corso…", "Moviendo…", "正在移动…",
         "이동하는 중…", "Bezig met verplaatsen…", "Movendo…", "Перемещение…", "正在移動…"},
        /* S_DELETING */
        {"削除中…", "Deleting…", "Suppression…", "Wird gelöscht …", "Eliminazione in corso…", "Eliminando…",
         "正在删除…", "삭제하는 중…", "Bezig met verwijderen…", "Excluindo…", "Удаление…", "正在刪除…"},
        /* S_DONE */
        {"完了", "Done", "Terminé", "Fertig", "Fatto", "Hecho", "完成", "완료", "Klaar", "Concluído", "Готово", "完成"},
        /* S_CANCELED */
        {"中止しました", "Canceled", "Annulé", "Abgebrochen", "Annullato", "Cancelado", "已取消", "취소됨",
         "Geannuleerd", "Cancelado", "Отменено", "已取消"},
        /* S_ERR_INVALID_NAME */
        {"使用できない名前です", "Invalid name", "Nom non valide", "Ungültiger Name", "Nome non valido",
         "Nombre no válido", "名称无效", "잘못된 이름", "Ongeldige naam", "Nome inválido", "Недопустимое имя", "名稱無效"},
        /* S_ERR_EXISTS */
        {"同じ名前の項目がすでにあります", "An item with this name already exists",
         "Un élément portant ce nom existe déjà", "Es gibt bereits ein Element mit diesem Namen",
         "Esiste già un elemento con questo nome", "Ya existe un elemento con este nombre", "已存在同名项目",
         "같은 이름의 항목이 이미 있습니다", "Er bestaat al een item met deze naam", "Já existe um item com este nome",
         "Элемент с таким именем уже существует", "已有同名項目"},
        /* S_ERR_READONLY */
        {"この場所は読み取り専用です", "This location is read-only", "Cet emplacement est en lecture seule",
         "Dieser Bereich ist schreibgeschützt", "Questa posizione è di sola lettura",
         "Esta ubicación es de solo lectura", "此位置为只读", "이 위치는 읽기 전용입니다",
         "Deze locatie is alleen-lezen", "Este local é somente leitura", "Это место доступно только для чтения",
         "此位置為唯讀"},
        /* S_ERR_INTO_ITSELF */
        {"フォルダーをそれ自身の中に入れることはできません", "A folder cannot be placed inside itself",
         "Un dossier ne peut pas être placé dans lui-même", "Ein Ordner kann nicht in sich selbst abgelegt werden",
         "Una cartella non può essere inserita in se stessa", "Una carpeta no puede colocarse dentro de sí misma",
         "文件夹不能放入其自身", "폴더를 자기 자신 안에 넣을 수 없습니다",
         "Een map kan niet in zichzelf worden geplaatst", "Uma pasta não pode ser colocada dentro de si mesma",
         "Нельзя поместить папку внутрь самой себя", "資料夾不能放入其自身"},
        /* S_ERR_FAILED */
        {"失敗しました: {0}", "Failed: {0}", "Échec : {0}", "Fehlgeschlagen: {0}", "Non riuscito: {0}", "Error: {0}",
         "失败：{0}", "실패: {0}", "Mislukt: {0}", "Falhou: {0}", "Ошибка: {0}", "失敗：{0}"},
        /* S_CLIPBOARD */
        {"「{0}」を貼り付けできます", "“{0}” ready to paste", "« {0} » prêt à être collé", "„{0}“ kann eingefügt werden",
         "«{0}» pronto da incollare", "«{0}» listo para pegar", "“{0}”可供粘贴", "'{0}' 붙여넣기 준비됨",
         "‘{0}’ klaar om te plakken", "“{0}” pronto para colar", "«{0}» готово к вставке", "「{0}」可供貼上"},
        /* S_PROGRESS_ITEMS */
        {"{0} / {1} 項目", "{0} of {1} items", "{0} sur {1} éléments", "{0} von {1} Elementen", "{0} di {1} elementi",
         "{0} de {1} elementos", "{0} / {1} 项", "{1}개 중 {0}개", "{0} van {1} items", "{0} de {1} itens",
         "{0} из {1}", "{0} / {1} 個項目"},

        // ------------------------------------------------------------ editor and viewers
        /* S_EDIT_LINE */
        {"行を編集", "Edit line", "Modifier la ligne", "Zeile bearbeiten", "Modifica riga", "Editar línea", "编辑此行",
         "줄 편집", "Regel bewerken", "Editar linha", "Изменить строку", "編輯此行"},
        /* S_INSERT_LINE */
        {"下に行を挿入", "Insert line below", "Insérer une ligne en dessous", "Neue Zeile darunter",
         "Inserisci riga sotto", "Insertar línea debajo", "在下方插入行", "아래에 줄 삽입", "Regel eronder invoegen",
         "Inserir linha abaixo", "Вставить строку ниже", "在下方插入行"},
        /* S_DELETE_LINE */
        {"行を削除", "Delete line", "Supprimer la ligne", "Zeile löschen", "Elimina riga", "Eliminar línea", "删除此行",
         "줄 삭제", "Regel verwijderen", "Excluir linha", "Удалить строку", "刪除此行"},
        /* S_DUP_LINE */
        {"行を複製", "Duplicate line", "Dupliquer la ligne", "Zeile duplizieren", "Duplica riga", "Duplicar línea",
         "复制此行", "줄 복제", "Regel dupliceren", "Duplicar linha", "Дублировать строку", "複製此行"},
        /* S_UNDO */
        {"元に戻す", "Undo", "Annuler l'action", "Rückgängig", "Annulla azione", "Deshacer", "撤销", "실행 취소",
         "Ongedaan maken", "Desfazer", "Отменить действие", "復原"},
        /* S_SAVE */
        {"保存", "Save", "Enregistrer", "Speichern", "Salva", "Guardar", "保存", "저장", "Opslaan", "Salvar",
         "Сохранить", "儲存"},
        /* S_CLOSE */
        {"閉じる", "Close", "Fermer", "Schließen", "Chiudi", "Cerrar", "关闭", "닫기", "Sluiten", "Fechar", "Закрыть",
         "關閉"},
        /* S_SAVED */
        {"保存しました", "Saved", "Enregistré", "Gespeichert", "Salvato", "Guardado", "已保存", "저장됨", "Opgeslagen",
         "Salvo", "Сохранено", "已儲存"},
        /* S_UNSAVED_TITLE */
        {"変更を保存しますか？", "Save changes?", "Enregistrer les modifications ?", "Änderungen speichern?",
         "Salvare le modifiche?", "¿Guardar los cambios?", "保存更改吗？", "변경 사항을 저장할까요?",
         "Wijzigingen opslaan?", "Salvar alterações?", "Сохранить изменения?", "儲存變更嗎？"},
        /* S_UNSAVED_MSG */
        {"「{0}」は変更されています。", "“{0}” has been changed.", "« {0} » a été modifié.", "„{0}“ wurde geändert.",
         "«{0}» è stato modificato.", "«{0}» se ha modificado.", "“{0}”已被修改。", "'{0}'이(가) 변경되었습니다.",
         "‘{0}’ is gewijzigd.", "“{0}” foi alterado.", "«{0}» изменён.", "「{0}」已被修改。"},
        /* S_DISCARD */
        {"破棄", "Discard", "Abandonner", "Verwerfen", "Scarta", "Descartar", "放弃", "버리기", "Negeren", "Descartar",
         "Не сохранять", "捨棄"},
        /* S_LINE_OF */
        {"{0} / {1} 行", "Line {0} of {1}", "Ligne {0} sur {1}", "Zeile {0} von {1}", "Riga {0} di {1}",
         "Línea {0} de {1}", "第 {0} 行，共 {1} 行", "{1}줄 중 {0}번째 줄", "Regel {0} van {1}", "Linha {0} de {1}",
         "Строка {0} из {1}", "第 {0} 行，共 {1} 行"},
        /* S_HEX_VIEW */
        {"バイナリファイル – 16進数表示", "Binary file – hex view", "Fichier binaire – vue hexadécimale",
         "Binärdatei – Hex-Ansicht", "File binario – vista esadecimale", "Archivo binario – vista hexadecimal",
         "二进制文件 – 十六进制视图", "바이너리 파일 – 16진수 보기", "Binair bestand – hexweergave",
         "Arquivo binário – visualização hexadecimal", "Двоичный файл – шестнадцатеричный вид", "二進位檔案 – 十六進位檢視"},
        /* S_HEX_LIMIT */
        {"先頭 {0} を表示", "Showing the first {0}", "Affichage des premiers {0}", "Die ersten {0} werden angezeigt",
         "Visualizzati i primi {0}", "Se muestran los primeros {0}", "显示前 {0}", "처음 {0} 표시",
         "De eerste {0} worden getoond", "Exibindo os primeiros {0}", "Показаны первые {0}", "顯示前 {0}"},
        /* S_TOO_LARGE */
        {"大きすぎて開けません（最大 {0}）", "Too large to open (max. {0})", "Trop volumineux (max. {0})",
         "Zu groß zum Öffnen (max. {0})", "Troppo grande (max. {0})", "Demasiado grande (máx. {0})",
         "文件过大（最大 {0}）", "너무 커서 열 수 없음 (최대 {0})", "Te groot om te openen (max. {0})",
         "Grande demais (máx. {0})", "Слишком большой файл (макс. {0})", "檔案過大（上限 {0}）"},
        /* S_CANNOT_READ */
        {"ファイルを読み込めません", "Cannot read this file", "Impossible de lire ce fichier",
         "Datei kann nicht gelesen werden", "Impossibile leggere il file", "No se puede leer el archivo",
         "无法读取此文件", "파일을 읽을 수 없습니다", "Bestand kan niet worden gelezen",
         "Não foi possível ler o arquivo", "Не удаётся прочитать файл", "無法讀取此檔案"},
        /* S_IMAGE */
        {"画像", "Image", "Image", "Bild", "Immagine", "Imagen", "图片", "이미지", "Afbeelding", "Imagem", "Изображение",
         "圖片"},
        /* S_TEXT_FILE */
        {"テキストファイル", "Text file", "Fichier texte", "Textdatei", "File di testo", "Archivo de texto", "文本文件",
         "텍스트 파일", "Tekstbestand", "Arquivo de texto", "Текстовый файл", "文字檔"},
        /* S_HINT_EDIT */
        {"編集", "Edit", "Modifier", "Bearbeiten", "Modifica", "Editar", "编辑", "편집", "Bewerken", "Editar",
         "Изменить", "編輯"},
        /* S_HINT_NEW_LINE */
        {"新しい行", "New line", "Nouvelle ligne", "Neue Zeile", "Nuova riga", "Nueva línea", "新行", "새 줄",
         "Nieuwe regel", "Nova linha", "Новая строка", "新行"},
        /* S_HINT_PREV_NEXT */
        {"前へ / 次へ", "Previous / next", "Précédent / suivant", "Vorheriges / Nächstes", "Precedente / successivo",
         "Anterior / siguiente", "上一个 / 下一个", "이전 / 다음", "Vorige / volgende", "Anterior / próximo",
         "Назад / вперёд", "上一個 / 下一個"},
        /* S_EMPTY_LINE */
        {"（空行）", "(empty line)", "(ligne vide)", "(leere Zeile)", "(riga vuota)", "(línea vacía)", "（空行）",
         "(빈 줄)", "(lege regel)", "(linha vazia)", "(пустая строка)", "（空行）"},
        /* S_OPT_BACKGROUND */
        {"バックグラウンドで動作を続ける", "Keep running in the background", "Continuer en arrière-plan",
         "Im Hintergrund weiterlaufen", "Continua in background", "Seguir en segundo plano", "在后台继续运行",
         "백그라운드에서 계속 실행", "Op de achtergrond blijven draaien", "Continuar em segundo plano",
         "Работать в фоне", "在背景繼續執行"},
        /* S_OPT_BACKGROUND_DESC */
        {"アプリを終了してもWii Uメニューやゲーム中にサーバーが動作します",
         "The server keeps running after you close the app, in the Wii U Menu and in games",
         "Le serveur continue après la fermeture de l'app, dans le menu Wii U et en jeu",
         "Server läuft nach dem Beenden der App weiter, im Wii-U-Menü und in Spielen",
         "Il server resta attivo dopo la chiusura dell'app, nel menu Wii U e nei giochi",
         "El servidor sigue activo al cerrar la app, en el menú de Wii U y en los juegos",
         "关闭应用后服务器仍在 Wii U 菜单和游戏中运行",
         "앱을 종료해도 Wii U 메뉴와 게임 중에 서버가 계속 실행됩니다",
         "De server blijft draaien na het sluiten van de app, in het Wii U-menu en in games",
         "O servidor continua ativo depois de fechar o app, no Menu Wii U e nos jogos",
         "Сервер работает и после закрытия приложения: в меню Wii U и в играх",
         "關閉應用程式後伺服器仍在 Wii U 選單和遊戲中執行"},
        /* S_BG_ACTIVE */
        {"有効 – アプリを終了するとAromaプラグインが引き継ぎます",
         "On – the Aroma plugin takes over when you leave the app",
         "Activé – le plugin Aroma prend le relais quand vous quittez l'app",
         "Aktiv – das Aroma-Plugin übernimmt, wenn du die App verlässt",
         "Attivo – il plugin Aroma subentra quando esci dall'app",
         "Activo – el plugin de Aroma toma el relevo al salir de la app", "已开启 – 退出应用后由 Aroma 插件接管",
         "켜짐 – 앱을 나가면 Aroma 플러그인이 이어서 실행합니다",
         "Aan – de Aroma-plug-in neemt het over als je de app verlaat",
         "Ativo – o plugin do Aroma assume quando você sai do app",
         "Включено – после выхода из приложения работу продолжит плагин Aroma",
         "已開啟 – 離開應用程式後由 Aroma 外掛接手"},
        /* S_BG_AFTER_REBOOT */
        {"本体を再起動すると有効になります", "Takes effect after restarting the console",
         "Effectif après le redémarrage de la console", "Wird nach einem Neustart der Konsole aktiv",
         "Attivo dopo il riavvio della console", "Se activa al reiniciar la consola", "重启主机后生效",
         "본체를 다시 시작하면 적용됩니다", "Actief na het herstarten van de console", "Ativo após reiniciar o console",
         "Заработает после перезапуска консоли", "重新啟動主機後生效"},
        /* S_BG_INSTALL_TITLE */
        {"SFTPiiUプラグインをインストールしますか？", "Install the SFTPiiU plugin?", "Installer le plugin SFTPiiU ?",
         "SFTPiiU-Plugin installieren?", "Installare il plugin SFTPiiU?", "¿Instalar el plugin SFTPiiU?",
         "安装 SFTPiiU 插件吗？", "SFTPiiU 플러그인을 설치할까요?", "SFTPiiU-plug-in installeren?",
         "Instalar o plugin SFTPiiU?", "Установить плагин SFTPiiU?", "安裝 SFTPiiU 外掛嗎？"},
        /* S_BG_INSTALL_MSG */
        {"AromaプラグインをSDカードにコピーします。同じパスワードとホストキーを使い、本体の再起動後に起動します。",
         "The Aroma plugin will be copied to the SD card. It uses the same password and host key and starts after a "
         "console restart.",
         "Le plugin Aroma sera copié sur la carte SD. Il utilise le même mot de passe et la même clé d'hôte et "
         "démarre après un redémarrage de la console.",
         "Das Aroma-Plugin wird auf die SD-Karte kopiert. Es nutzt dasselbe Passwort und denselben Host-Key und "
         "startet nach einem Neustart der Konsole.",
         "Il plugin Aroma verrà copiato sulla scheda SD. Usa la stessa password e la stessa chiave host e si avvia "
         "dopo il riavvio della console.",
         "El plugin de Aroma se copiará a la tarjeta SD. Usa la misma contraseña y clave de host y se inicia tras "
         "reiniciar la consola.",
         "将把 Aroma 插件复制到 SD 卡。它使用相同的密码和主机密钥，重启主机后启动。",
         "Aroma 플러그인을 SD 카드에 복사합니다. 같은 비밀번호와 호스트 키를 사용하며 본체를 다시 시작하면 실행됩니다.",
         "De Aroma-plug-in wordt naar de SD-kaart gekopieerd. Hij gebruikt hetzelfde wachtwoord en dezelfde "
         "hostsleutel en start na een herstart van de console.",
         "O plugin do Aroma será copiado para o cartão SD. Ele usa a mesma senha e chave de host e inicia após "
         "reiniciar o console.",
         "Плагин Aroma будет скопирован на SD-карту. Он использует тот же пароль и ключ хоста и запустится после "
         "перезапуска консоли.",
         "將把 Aroma 外掛複製到 SD 卡。它使用相同的密碼和主機金鑰，重新啟動主機後啟動。"},
        /* S_INSTALL */
        {"インストール", "Install", "Installer", "Installieren", "Installa", "Instalar", "安装", "설치", "Installeren",
         "Instalar", "Установить", "安裝"},
        /* S_BG_INSTALLED */
        {"プラグインをインストールしました。本体を再起動してください", "Plugin installed – restart the console",
         "Plugin installé – redémarrez la console", "Plugin installiert – starte die Konsole neu",
         "Plugin installato – riavvia la console", "Plugin instalado – reinicia la consola", "插件已安装，请重启主机",
         "플러그인을 설치했습니다. 본체를 다시 시작하세요", "Plug-in geïnstalleerd – herstart de console",
         "Plugin instalado – reinicie o console", "Плагин установлен – перезапустите консоль",
         "外掛已安裝，請重新啟動主機"},
        /* S_BG_OFF */
        {"サーバーはアプリを開いている間だけ動作します", "The server now only runs while the app is open",
         "Le serveur ne tourne plus que lorsque l'app est ouverte",
         "Der Server läuft jetzt nur noch, solange die App offen ist",
         "Ora il server funziona solo mentre l'app è aperta", "Ahora el servidor solo funciona con la app abierta",
         "服务器现在只在应用打开时运行", "이제 앱이 열려 있을 때만 서버가 실행됩니다",
         "De server draait nu alleen terwijl de app open is", "Agora o servidor só funciona com o app aberto",
         "Теперь сервер работает только при открытом приложении", "伺服器現在只在應用程式開啟時執行"},
};

} // namespace

Lang console_language() {
    // IPC buffers must be 64-byte aligned.
    alignas(0x40) static uint32_t value;
    alignas(0x40) static UCSysConfig setting;
    value           = (uint32_t) Lang::EN;
    UCHandle handle = UCOpen();
    if (handle >= 0) {
        setting = UCSysConfig{};
        // "cafe.language" holds the system language as an unsigned int.
        const char name[] = "cafe.language";
        for (size_t i = 0; i < sizeof(name); i++) {
            setting.name[i] = name[i];
        }
        setting.access   = 0;
        setting.dataType = UC_DATATYPE_UNSIGNED_INT;
        setting.dataSize = sizeof(value);
        setting.data     = &value;
        if (UCReadSysConfig(handle, 1, &setting) != 0) {
            value = (uint32_t) Lang::EN;
        }
        UCClose(handle);
    }
    if (value >= (uint32_t) Lang::Count) {
        value = (uint32_t) Lang::EN;
    }
    return (Lang) value;
}

void set_language(Lang lang) {
    gLang = lang;
}

Lang language() {
    return gLang;
}

const char *tr(Str id) {
    const char *s = kTexts[id][(int) gLang];
    if (!s) {
        s = kTexts[id][(int) Lang::EN];
    }
    return s ? s : "?";
}

int i18n_missing() {
    int missing = 0;
    for (int id = 0; id < S_COUNT; id++) {
        for (int l = 0; l < (int) Lang::Count; l++) {
            if (!kTexts[id][l]) {
                missing++;
            }
        }
    }
    return missing;
}

std::string trf(Str id, const std::string &a0, const std::string &a1) {
    std::string out;
    for (const char *p = tr(id); *p; p++) {
        if (p[0] == '{' && (p[1] == '0' || p[1] == '1') && p[2] == '}') {
            out += p[1] == '0' ? a0 : a1;
            p += 2;
        } else {
            out += *p;
        }
    }
    return out;
}

int font_type_for(Lang lang) {
    switch (lang) {
        case Lang::ZH:
            return OS_SHAREDDATATYPE_FONT_CHINESE;
        case Lang::KO:
            return OS_SHAREDDATATYPE_FONT_KOREAN;
        case Lang::TW:
            return OS_SHAREDDATATYPE_FONT_TAIWANESE;
        default:
            return OS_SHAREDDATATYPE_FONT_STANDARD;
    }
}

char decimal_separator() {
    switch (gLang) {
        case Lang::FR:
        case Lang::DE:
        case Lang::IT:
        case Lang::ES:
        case Lang::NL:
        case Lang::PT:
        case Lang::RU:
            return ',';
        default:
            return '.';
    }
}
