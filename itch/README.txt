ผีหลอก (Rules of Horror: สถานีรอรถบนเขา)
==========================================

เกมสยองขวัญมุมมองบุคคลที่หนึ่ง เล่นจบในราว 10–12 นาที — ใส่หูฟังจะดีที่สุด
A first-person horror game, about 10–12 minutes long. Play with headphones.

ต้องการ: macOS 11 ขึ้นไป (Apple Silicon หรือ Intel)
Requires: macOS 11 or newer (Apple Silicon or Intel)


วิธีเปิดเกมครั้งแรก
-------------------
เกมนี้ไม่ได้ลงทะเบียนกับ Apple ครั้งแรก macOS จึงจะไม่ยอมให้เปิด ให้ทำแบบนี้:

1. ลาก grdc-horror.app ไปไว้ในโฟลเดอร์ Applications (หรือที่ไหนก็ได้)
2. ดับเบิลคลิกเปิด 1 ครั้ง ถ้าขึ้นว่า "ไม่สามารถเปิดได้" ให้กด Done / ยกเลิก
3. เปิด System Settings > Privacy & Security เลื่อนลงไปจะเจอ "grdc-horror was blocked"
   กด "Open Anyway" แล้วใส่รหัสผ่าน
   (macOS 14 หรือเก่ากว่า: คลิกขวาที่แอป > Open > Open ได้เลย)

ถ้ายังเปิดไม่ได้ ให้เปิด Terminal แล้วพิมพ์ (แก้ path ให้ตรงกับที่วางแอปไว้):
    xattr -dr com.apple.quarantine /Applications/grdc-horror.app


First launch (macOS)
--------------------
The app is not notarized by Apple, so macOS blocks it the first time:

1. Move grdc-horror.app to Applications (or anywhere).
2. Double-click it once. If macOS says it can't be opened, press Done.
3. Open System Settings > Privacy & Security, scroll down to "grdc-horror was blocked",
   press "Open Anyway" and confirm.
   (macOS 14 or older: right-click the app > Open > Open.)

Still blocked? In Terminal (adjust the path):
    xattr -dr com.apple.quarantine /Applications/grdc-horror.app


ปุ่ม / Controls
---------------
คลิกในหน้าต่าง      เริ่ม / จับเมาส์              Click        start / capture mouse
เมาส์               มอง                          Mouse        look
W A S D             เดิน                          WASD         walk
Shift               วิ่ง                          Shift        run
Tab                 หยิบ / เก็บโทรศัพท์            Tab          phone out / away
E                   อ่านข้อความ คุย ซ่อน นั่ง หยิบ  E            read, talk, hide, sit, use
F                   ไฟฉาย (กินแบต)                F            flashlight (uses battery)
ลูกกลิ้งเมาส์        เลื่อนข้อความ                  Mouse wheel  scroll messages
1–9                 เลือกคำตอบ                    1–9          answer
ลูกศร + Space        มินิเกมกลั้นหายใจ              Arrows+Space breath mini-game
M                   เปิด / ปิดเสียง                M            mute
Esc                 ปล่อยเมาส์ (กดอีกครั้ง = ออก)   Esc          free mouse (again = quit)
