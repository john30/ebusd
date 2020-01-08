pipeline {
    environment {
        registry = "comdata456/ebusd"
        registryCredential = 'docker-hub-credentials'
    }

    agent {
        docker {
            image 'docker' 
            args '-v $HOME/.m2:/root/.m2 -v /root/.ssh:/root/.ssh -v /run/docker.sock:/run/docker.sock' 
        }
    }



    stages {

        
        stage('Make Container') {

            steps {
                sh "docker build -t comdata456/ebusd:${env.BUILD_ID} contrib/docker"
                sh "docker tag comdata456/ebusd:${env.BUILD_ID} comdata456/ebusd:latest"
            }
        }
          stage('Publish') {

            steps {
                withCredentials([usernamePassword(credentialsId: 'docker-hub-credentials', usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
                    sh "docker login -u ${USERNAME} -p ${PASSWORD}"
                    sh "docker push comdata456/ebusd:${env.BUILD_ID}"
                    sh "docker push comdata456/ebusd:latest"
                }
            }
        }
    }
}
