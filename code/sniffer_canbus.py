import os
import sys
import can

# === SÉCURITÉ POUR ANACONDA / WINDOWS 11 ===
# On force PyUSB à charger la DLL libusb-1.0.dll qui est dans votre dossier
import usb.core
import usb.backend.libusb1

current_dir = os.path.dirname(os.path.abspath(__file__))
dll_path = os.path.join(current_dir, "libusb-1.0.dll")

if os.path.exists(dll_path):
    # On injecte manuellement le backend en pointant sur votre DLL locale
    backend = usb.backend.libusb1.get_backend(find_library=lambda x: dll_path)
    print(f"[Info] Backend libusb forcé localement depuis : {dll_path}")
else:
    print(f"[Attention] Le fichier libusb-1.0.dll est introuvable dans {current_dir} !")
    backend = None
# ===========================================

print("=== DÉMARRAGE DU SNIFFER CAN ===")
try:
    # On passe le backend libusb configuré à l'interface gs_usb
    bus = can.interface.Bus(
        interface='gs_usb', 
        channel=0, 
        bitrate=250000, 
        listen_only=True,
        backend=backend # L'argument magique pour PyUSB
    )
    print("Clé détectée avec succès ! Mode 'Listen Only' activé.")
    print("Enregistrement en cours... Appuyez sur Ctrl+C pour arrêter.")
    
    with can.ASCWriter("session_can_stable.asc") as logger:
        for msg in bus:
            logger.on_message_received(msg)
            print(f"ID: {hex(msg.arbitration_id)} | Data: {msg.data.hex()}")

except KeyboardInterrupt:
    print("\nArrêt de l'enregistrement demandé par l'utilisateur.")
except Exception as e:
    print(f"\nUne erreur est survenue : {e}")
    sys.exit(1)