import sys
from PyQt5.QtWidgets import QApplication, QMainWindow, QAction, QStackedWidget, QWidget, QVBoxLayout, QLabel, QFileDialog, QLineEdit, QPushButton, QHBoxLayout, QCheckBox, QComboBox, QStatusBar
from PyQt5.QtGui import QIcon

class BackupApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.initUI()

    def initUI(self):
        self.setWindowTitle('数据备份软件')
        self.setGeometry(100, 100, 800, 600)

        # 创建菜单栏
        menubar = self.menuBar()

        fileMenu = menubar.addMenu('文件')
        openFile = QAction(QIcon('icons/open.png'), '打开', self)
        openFile.triggered.connect(self.open_file)
        fileMenu.addAction(openFile)

        saveFile = QAction(QIcon('icons/save.png'), '保存', self)
        saveFile.triggered.connect(self.save_file)
        fileMenu.addAction(saveFile)

        exitApp = QAction(QIcon('icons/exit.png'), '退出', self)
        exitApp.triggered.connect(self.close)
        fileMenu.addAction(exitApp)

        # 创建功能菜单
        functionMenu = menubar.addMenu('功能')

        fileTypeAction = QAction('文件类型支持', self)
        fileTypeAction.triggered.connect(lambda: self.switch_page(0))
        functionMenu.addAction(fileTypeAction)

        metadataAction = QAction('元数据支持', self)
        metadataAction.triggered.connect(lambda: self.switch_page(1))
        functionMenu.addAction(metadataAction)

        customBackupAction = QAction('自定义备份', self)
        customBackupAction.triggered.connect(lambda: self.switch_page(2))
        functionMenu.addAction(customBackupAction)

        # 创建堆叠窗口
        self.stackedWidget = QStackedWidget()
        self.setCentralWidget(self.stackedWidget)

        self.create_file_type_page()
        self.create_metadata_page()
        self.create_custom_backup_page()
        # 其他功能页面
        # ...

        # 创建状态栏
        self.statusBar = QStatusBar()
        self.setStatusBar(self.statusBar)

    def create_file_type_page(self):
        page = QWidget()
        layout = QVBoxLayout()
        layout.addWidget(QLabel('文件类型支持'))
        # 添加更多控件
        layout.addWidget(QCheckBox('支持管道'))
        layout.addWidget(QCheckBox('支持软链接'))
        layout.addWidget(QCheckBox('支持硬链接'))
        page.setLayout(layout)
        self.stackedWidget.addWidget(page)

    def create_metadata_page(self):
        page = QWidget()
        layout = QVBoxLayout()
        layout.addWidget(QLabel('元数据支持'))
        # 添加更多控件
        layout.addWidget(QCheckBox('支持属主'))
        layout.addWidget(QCheckBox('支持时间'))
        layout.addWidget(QCheckBox('支持权限'))
        page.setLayout(layout)
        self.stackedWidget.addWidget(page)

    def create_custom_backup_page(self):
        page = QWidget()
        layout = QVBoxLayout()
        layout.addWidget(QLabel('自定义备份'))
        # 添加更多控件
        layout.addWidget(QLabel('路径:'))
        layout.addWidget(QLineEdit())
        layout.addWidget(QLabel('类型:'))
        layout.addWidget(QComboBox())
        layout.addWidget(QLabel('名字:'))
        layout.addWidget(QLineEdit())
        layout.addWidget(QLabel('时间:'))
        layout.addWidget(QLineEdit())
        layout.addWidget(QLabel('尺寸:'))
        layout.addWidget(QLineEdit())
        page.setLayout(layout)
        self.stackedWidget.addWidget(page)

    def switch_page(self, index):
        self.stackedWidget.setCurrentIndex(index)
        self.statusBar.showMessage(f'切换到页面 {index}')

    def open_file(self):
        options = QFileDialog.Options()
        fileName, _ = QFileDialog.getOpenFileName(self, "打开文件", "", "All Files (*);;Python Files (*.py)", options=options)
        if fileName:
            self.statusBar.showMessage(f"打开文件: {fileName}")

    def save_file(self):
        options = QFileDialog.Options()
        fileName, _ = QFileDialog.getSaveFileName(self, "保存文件", "", "All Files (*);;Python Files (*.py)", options=options)
        if fileName:
            self.statusBar.showMessage(f"保存文件: {fileName}")

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = BackupApp()
    ex.show()
    sys.exit(app.exec_())