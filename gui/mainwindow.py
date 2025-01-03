from PySide6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, 
                                 QPushButton, QFileDialog, QLabel, 
                                 QProgressBar, QMessageBox)
from PySide6.QtCore import Qt, Slot

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ByteEnclave 备份工具")
        self.setup_ui()

    def setup_ui(self):
        # 创建中心部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # 源目录选择
        self.source_label = QLabel("源目录：未选择")
        self.select_source_btn = QPushButton("选择源目录")
        self.select_source_btn.clicked.connect(self.select_source_directory)

        # 目标目录选择
        self.target_label = QLabel("目标目录：未选择")
        self.select_target_btn = QPushButton("选择目标目录")
        self.select_target_btn.clicked.connect(self.select_target_directory)

        # 备份和还原按钮
        self.backup_btn = QPushButton("开始备份")
        self.backup_btn.clicked.connect(self.start_backup)
        self.restore_btn = QPushButton("开始还原")
        self.restore_btn.clicked.connect(self.start_restore)

        # 进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)

        # 添加部件到布局
        layout.addWidget(self.source_label)
        layout.addWidget(self.select_source_btn)
        layout.addWidget(self.target_label)
        layout.addWidget(self.select_target_btn)
        layout.addWidget(self.backup_btn)
        layout.addWidget(self.restore_btn)
        layout.addWidget(self.progress_bar)

        # 设置窗口大小
        self.setMinimumSize(500, 300)

    @Slot()
    def select_source_directory(self):
        directory = QFileDialog.getExistingDirectory(self, "选择源目录")
        if directory:
            self.source_label.setText(f"源目录：{directory}")

    @Slot()
    def select_target_directory(self):
        directory = QFileDialog.getExistingDirectory(self, "选择目标目录")
        if directory:
            self.target_label.setText(f"目标目录：{directory}")

    @Slot()
    def start_backup(self):
        # TODO: 实现备份逻辑
        QMessageBox.information(self, "提示", "开始备份...")

    @Slot()
    def start_restore(self):
        # TODO: 实现还原逻辑
        QMessageBox.information(self, "提示", "开始还原...") 