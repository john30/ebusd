pipeline {
    triggers {
        cron('H 3 * * *')
        pollSCM('H */3 * * 1-5')
    }
    
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
                sh "docker build -t ${registry}:${env.BUILD_ID} contrib/docker"
                sh "docker tag ${registry}:${env.BUILD_ID} ${registry}:latest"
            }
        }
          stage('Publish') {

            steps {
                withCredentials([usernamePassword(credentialsId: 'docker-hub-credentials', usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
                    sh "docker login -u ${USERNAME} -p ${PASSWORD}"
                    sh "docker push ${registry}:${env.BUILD_ID}"
                    sh "docker push ${registry}:latest"
                }
            }
        }
    }
}
